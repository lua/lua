/*
** $Id: lvm.c,v 1.118 2000/06/27 19:00:36 roberto Exp roberto $
** Lua virtual machine
** See Copyright Notice in lua.h
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LUA_REENTRANT

#include "lua.h"

#include "lapi.h"
#include "lauxlib.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lvm.h"


#ifdef OLD_ANSI
#define strcoll(a,b)	strcmp(a,b)
#endif



/*
** Extra stack size to run a function:
** TAG_LINE(1), NAME(1), TM calls(3) (plus some extra...)
*/
#define	EXTRA_STACK	8



int luaV_tonumber (TObject *obj) {  /* LUA_NUMBER */
  if (ttype(obj) != TAG_STRING)
    return 1;
  else {
    if (!luaO_str2d(svalue(obj), &nvalue(obj)))
      return 2;
    ttype(obj) = TAG_NUMBER;
    return 0;
  }
}


int luaV_tostring (lua_State *L, TObject *obj) {  /* LUA_NUMBER */
  if (ttype(obj) != TAG_NUMBER)
    return 1;
  else {
    char s[32];  /* 16 digits, sign, point and \0  (+ some extra...) */
    sprintf(s, "%.16g", (double)nvalue(obj));
    tsvalue(obj) = luaS_new(L, s);
    ttype(obj) = TAG_STRING;
    return 0;
  }
}


static void traceexec (lua_State *L, StkId base, StkId top, lua_Hook linehook) {
  CallInfo *ci = infovalue(base-1);
  int *lines = ci->func->f.l->lines;
  int pc = (*ci->pc - 1) - ci->func->f.l->code;
  if (lines) {
    /* calls linehook when enters a new line or jumps back (loop) */
    if (lines[pc] != ci->line || pc <= ci->lastpc) {
      ci->line = lines[pc];
      L->top = top;
      luaD_lineHook(L, base-2, lines[pc], linehook);
    }
  }
  ci->lastpc = pc;
}


static Closure *luaV_closure (lua_State *L, lua_Type t, int nelems) {
  Closure *c = luaF_newclosure(L, nelems);
  L->top -= nelems;
  while (nelems--)
    c->upvalue[nelems] = *(L->top+nelems);
  ttype(L->top) = t;
  clvalue(L->top) = c;
  incr_top;
  return c;
}


void luaV_Cclosure (lua_State *L, lua_CFunction c, int nelems) {
  Closure *cl = luaV_closure(L, TAG_CCLOSURE, nelems);
  cl->f.c = c;
}


void luaV_Lclosure (lua_State *L, Proto *l, int nelems) {
  Closure *cl = luaV_closure(L, TAG_LCLOSURE, nelems);
  cl->f.l = l;
}


/*
** Function to index a table.
** Receives the table at top-2 and the index at top-1.
*/
void luaV_gettable (lua_State *L, StkId top) {
  StkId table = top-2;
  const TObject *im;
  if (ttype(table) != TAG_TABLE) {  /* not a table, get gettable TM */
    im = luaT_getimbyObj(L, table, IM_GETTABLE);
    if (ttype(im) == TAG_NIL) {
      L->top = top;
      luaG_indexerror(L, table);
    }
  }
  else {  /* object is a table... */
    int tg = hvalue(table)->htag;
    im = luaT_getim(L, tg, IM_GETTABLE);
    if (ttype(im) == TAG_NIL) {  /* and does not have a `gettable' TM */
      const TObject *h = luaH_get(L, hvalue(table), table+1);
      if (ttype(h) == TAG_NIL &&
          (ttype(im=luaT_getim(L, tg, IM_INDEX)) != TAG_NIL)) {
        /* result is nil and there is an `index' tag method */
        L->top = top;
        luaD_callTM(L, im, 2, 1);  /* calls it */
      }
      else
        *table = *h;  /* `push' result into table position */
      return;
    }
    /* else it has a `gettable' TM, go through to next command */
  }
  /* object is not a table, or it has a `gettable' TM */
  L->top = top;
  luaD_callTM(L, im, 2, 1);
}


/*
** Receives table at *t, index at *(t+1) and value at `top'.
*/
void luaV_settable (lua_State *L, StkId t, StkId top) {
  const TObject *im;
  if (ttype(t) != TAG_TABLE) {  /* not a table, get `settable' method */
    L->top = top;
    im = luaT_getimbyObj(L, t, IM_SETTABLE);
    if (ttype(im) == TAG_NIL)
      luaG_indexerror(L, t);
  }
  else {  /* object is a table... */
    im = luaT_getim(L, hvalue(t)->htag, IM_SETTABLE);
    if (ttype(im) == TAG_NIL) {  /* and does not have a `settable' method */
      *luaH_set(L, hvalue(t), t+1) = *(top-1);
      return;
    }
    /* else it has a `settable' method, go through to next command */
  }
  /* object is not a table, or it has a `settable' method */
  /* prepare arguments and call the tag method */
  luaD_checkstack(L, 3);
  *(top+2) = *(top-1);
  *(top+1) = *(t+1);
  *(top) = *t;
  *(top-1) = *im;
  L->top = top+3;
  luaD_call(L, top-1, 0);
}


void luaV_getglobal (lua_State *L, TString *s, StkId top) {
  const TObject *value = luaH_getstr(L->gt, s);
  TObject *im = luaT_getimbyObj(L, value, IM_GETGLOBAL);
  if (ttype(im) == TAG_NIL)  /* is there a tag method? */
    *top = *value;  /* default behavior */
  else {  /* tag method */
    luaD_checkstack(L, 3);
    *top = *im;
    ttype(top+1) = TAG_STRING;
    tsvalue(top+1) = s;  /* global name */
    *(top+2) = *value;
    L->top = top+3;
    luaD_call(L, top, 1);
  }
}


void luaV_setglobal (lua_State *L, TString *s, StkId top) {
  const TObject *oldvalue = luaH_getstr(L->gt, s);
  const TObject *im = luaT_getimbyObj(L, oldvalue, IM_SETGLOBAL);
  if (ttype(im) == TAG_NIL) {  /* is there a tag method? */
    if (oldvalue != &luaO_nilobject) {
      /* cast to remove `const' is OK, because `oldvalue' != luaO_nilobject */
      *(TObject *)oldvalue = *(top-1);
    }
    else {
      TObject key;
      ttype(&key) = TAG_STRING;
      tsvalue(&key) = s;
      *luaH_set(L, L->gt, &key) = *(top-1);
    }
  }
  else {
    luaD_checkstack(L, 3);
    *(top+2) = *(top-1);  /* new value */
    *(top+1) = *oldvalue;
    ttype(top) = TAG_STRING;
    tsvalue(top) = s;
    *(top-1) = *im;
    L->top = top+3;
    luaD_call(L, top-1, 0);
  }
}


static void call_binTM (lua_State *L, StkId top, IMS event, const char *msg) {
  /* try first operand */
  const TObject *im = luaT_getimbyObj(L, top-2, event);
  L->top = top;
  if (ttype(im) == TAG_NIL) {
    im = luaT_getimbyObj(L, top-1, event);  /* try second operand */
    if (ttype(im) == TAG_NIL) {
      im = luaT_getim(L, 0, event);  /* try a `global' method */
      if (ttype(im) == TAG_NIL)
        lua_error(L, msg);
    }
  }
  lua_pushstring(L, luaT_eventname[event]);
  luaD_callTM(L, im, 3, 1);
}


static void call_arith (lua_State *L, StkId top, IMS event) {
  call_binTM(L, top, event, "unexpected type in arithmetic operation");
}


static int luaV_strcomp (const TString *ls, const TString *rs) {
  const char *l = ls->str;
  size_t ll = ls->u.s.len;
  const char *r = rs->str;
  size_t lr = rs->u.s.len;
  for (;;) {
    int temp = strcoll(l, r);
    if (temp != 0) return temp;
    else {  /* strings are equal up to a '\0' */
      size_t len = strlen(l);  /* index of first '\0' in both strings */
      if (len == ll)  /* l is finished? */
        return (len == lr) ? 0 : -1;  /* l is equal or smaller than r */
      else if (len == lr)  /* r is finished? */
        return 1;  /* l is greater than r (because l is not finished) */
      /* both strings longer than `len'; go on comparing (after the '\0') */
      len++;
      l += len; ll -= len; r += len; lr -= len;
    }
  }
}


int luaV_lessthan (lua_State *L, const TObject *l, const TObject *r, StkId top) {
  if (ttype(l) == TAG_NUMBER && ttype(r) == TAG_NUMBER)
    return (nvalue(l) < nvalue(r));
  else if (ttype(l) == TAG_STRING && ttype(r) == TAG_STRING)
    return (luaV_strcomp(tsvalue(l), tsvalue(r)) < 0);
  else {  /* call TM */
    luaD_checkstack(L, 2);
    *top++ = *l;
    *top++ = *r;
    call_binTM(L, top, IM_LT, "unexpected type in comparison");
    L->top--;
    return (ttype(L->top) != TAG_NIL);
  }
}


static void strconc (lua_State *L, int total, StkId top) {
  do {
    int n = 2;  /* number of elements handled in this pass (at least 2) */
    if (tostring(L, top-2) || tostring(L, top-1))
      call_binTM(L, top, IM_CONCAT, "unexpected type for concatenation");
    else if (tsvalue(top-1)->u.s.len > 0) {  /* if len=0, do nothing */
      /* at least two string values; get as many as possible */
      lint32 tl = (lint32)tsvalue(top-1)->u.s.len + 
                  (lint32)tsvalue(top-2)->u.s.len;
      char *buffer;
      int i;
      while (n < total && !tostring(L, top-n-1)) {  /* collect total length */
        tl += tsvalue(top-n-1)->u.s.len;
        n++;
      }
      if (tl > MAX_SIZET) lua_error(L, "string size overflow");
      buffer = luaL_openspace(L, tl);
      tl = 0;
      for (i=n; i>0; i--) {  /* concat all strings */
        size_t l = tsvalue(top-i)->u.s.len;
        memcpy(buffer+tl, tsvalue(top-i)->str, l);
        tl += l;
      }
      tsvalue(top-n) = luaS_newlstr(L, buffer, tl);
    }
    total -= n-1;  /* got `n' strings to create 1 new */
    top -= n-1;
  } while (total > 1);  /* repeat until only 1 result left */
}


void luaV_pack (lua_State *L, StkId firstelem, int nvararg, TObject *tab) {
  int i;
  Hash *htab;
  htab = hvalue(tab) = luaH_new(L, nvararg+1);  /* +1 for field `n' */
  ttype(tab) = TAG_TABLE;
  for (i=0; i<nvararg; i++)
    *luaH_setint(L, htab, i+1) = *(firstelem+i);
  /* store counter in field `n' */
  luaH_setstrnum(L, htab, luaS_new(L, "n"), nvararg);
}


static void adjust_varargs (lua_State *L, StkId base, int nfixargs) {
  TObject arg;
  int nvararg = (L->top-base) - nfixargs;
  if (nvararg < 0) {
    luaV_pack(L, base, 0, &arg);
    luaD_adjusttop(L, base, nfixargs);
  }
  else {
    luaV_pack(L, base+nfixargs, nvararg, &arg);
    L->top = base+nfixargs;
  }
  *L->top++ = arg;
}


/*
** Executes the given Lua function. Parameters are between [base,top).
** Returns n such that the the results are between [n,top).
*/
StkId luaV_execute (lua_State *L, const Closure *cl, StkId base) {
  const Proto *tf = cl->f.l;
  StkId top;  /* keep top local, for performance */
  const Instruction *pc = tf->code;
  TString **kstr = tf->kstr;
  lua_Hook linehook = L->linehook;
  infovalue(base-1)->pc = &pc;
  luaD_checkstack(L, tf->maxstacksize+EXTRA_STACK);
  if (tf->is_vararg) {  /* varargs? */
    adjust_varargs(L, base, tf->numparams);
    luaC_checkGC(L);
  }
  else
    luaD_adjusttop(L, base, tf->numparams);
  top = L->top;
  /* main loop of interpreter */
  for (;;) {
    const Instruction i = *pc++;
    if (linehook)
      traceexec(L, base, top, linehook);
    switch (GET_OPCODE(i)) {

      case OP_END:
        return L->top;  /* no results */

      case OP_RETURN:
        L->top = top;
        return base+GETARG_U(i);

      case OP_CALL:
        L->top = top;
        luaD_call(L, base+GETARG_A(i), GETARG_B(i));
        top = L->top;
        break;

      case OP_TAILCALL:
        L->top = top;
        luaD_call(L, base+GETARG_A(i), MULT_RET);
        return base+GETARG_B(i);

      case OP_PUSHNIL: {
        int n = GETARG_U(i);
        LUA_ASSERT(L, n>0, "invalid argument");
        do {
          ttype(top++) = TAG_NIL;
        } while (--n > 0);
        break;
      }

      case OP_POP:
        top -= GETARG_U(i);
        break;

      case OP_PUSHINT:
        ttype(top) = TAG_NUMBER;
        nvalue(top) = (Number)GETARG_S(i);
        top++;
        break;

      case OP_PUSHSTRING:
        ttype(top) = TAG_STRING;
        tsvalue(top) = kstr[GETARG_U(i)];
        top++;
        break;

      case OP_PUSHNUM:
        ttype(top) = TAG_NUMBER;
        nvalue(top) = tf->knum[GETARG_U(i)];
        top++;
        break;

      case OP_PUSHNEGNUM:
        ttype(top) = TAG_NUMBER;
        nvalue(top) = -tf->knum[GETARG_U(i)];
        top++;
        break;

      case OP_PUSHUPVALUE:
        *top++ = cl->upvalue[GETARG_U(i)];
        break;

      case OP_GETLOCAL:
        *top++ = *(base+GETARG_U(i));
        break;

      case OP_GETGLOBAL:
        luaV_getglobal(L, kstr[GETARG_U(i)], top);
        top++;
        break;

      case OP_GETTABLE:
        luaV_gettable(L, top);
        top--;
        break;

      case OP_GETDOTTED:
        ttype(top) = TAG_STRING;
        tsvalue(top++) = kstr[GETARG_U(i)];
        luaV_gettable(L, top);
        top--;
        break;

      case OP_GETINDEXED:
        *top++ = *(base+GETARG_U(i));
        luaV_gettable(L, top);
        top--;
        break;

      case OP_PUSHSELF: {
        TObject receiver;
        receiver = *(top-1);
        ttype(top) = TAG_STRING;
        tsvalue(top++) = kstr[GETARG_U(i)];
        luaV_gettable(L, top);
        *(top-1) = receiver;
        break;
      }

      case OP_CREATETABLE:
        L->top = top;
        luaC_checkGC(L);
        hvalue(top) = luaH_new(L, GETARG_U(i));
        ttype(top) = TAG_TABLE;
        top++;
        break;

      case OP_SETLOCAL:
        *(base+GETARG_U(i)) = *(--top);
        break;

      case OP_SETGLOBAL:
        luaV_setglobal(L, kstr[GETARG_U(i)], top);
        top--;
        break;

      case OP_SETTABLE:
        luaV_settable(L, top-GETARG_A(i), top);
        top -= GETARG_B(i);  /* pop values */
        break;

      case OP_SETLIST: {
        int aux = GETARG_A(i) * LFIELDS_PER_FLUSH;
        int n = GETARG_B(i);
        Hash *arr = hvalue(top-n-1);
        L->top = top-n;  /* final value of `top' (in case of errors) */
        for (; n; n--)
          *luaH_setint(L, arr, n+aux) = *(--top);
        break;
      }

      case OP_SETMAP: {
        int n = GETARG_U(i);
        StkId finaltop = top-2*n;
        Hash *arr = hvalue(finaltop-1);
        L->top = finaltop;  /* final value of `top' (in case of errors) */
        for (; n; n--) {
          top-=2;
          *luaH_set(L, arr, top) = *(top+1);
        }
        break;
      }

      case OP_ADD:
        if (tonumber(top-1) || tonumber(top-2))
          call_arith(L, top, IM_ADD);
        else
          nvalue(top-2) += nvalue(top-1);
        top--;
        break;

      case OP_ADDI:
        if (tonumber(top-1)) {
          ttype(top) = TAG_NUMBER;
          nvalue(top) = (Number)GETARG_S(i);
          call_arith(L, top+1, IM_ADD);
        }
        else
          nvalue(top-1) += (Number)GETARG_S(i);
        break;

      case OP_SUB:
        if (tonumber(top-1) || tonumber(top-2))
          call_arith(L, top, IM_SUB);
        else
          nvalue(top-2) -= nvalue(top-1);
        top--;
        break;

      case OP_MULT:
        if (tonumber(top-1) || tonumber(top-2))
          call_arith(L, top, IM_MUL);
        else
          nvalue(top-2) *= nvalue(top-1);
        top--;
        break;

      case OP_DIV:
        if (tonumber(top-1) || tonumber(top-2))
          call_arith(L, top, IM_DIV);
        else
          nvalue(top-2) /= nvalue(top-1);
        top--;
        break;

      case OP_POW:
        call_binTM(L, top, IM_POW, "undefined operation");
        top--;
        break;

      case OP_CONCAT: {
        int n = GETARG_U(i);
        strconc(L, n, top);
        top -= n-1;
        L->top = top;
        luaC_checkGC(L);
        break;
      }

      case OP_MINUS:
        if (tonumber(top-1)) {
          ttype(top) = TAG_NIL;
          call_arith(L, top+1, IM_UNM);
        }
        else
          nvalue(top-1) = -nvalue(top-1);
        break;

      case OP_NOT:
        ttype(top-1) =
           (ttype(top-1) == TAG_NIL) ? TAG_NUMBER : TAG_NIL;
        nvalue(top-1) = 1;
        break;

      case OP_JMPNE:
        top -= 2;
        if (!luaO_equalObj(top, top+1)) pc += GETARG_S(i);
        break;

      case OP_JMPEQ:
        top -= 2;
        if (luaO_equalObj(top, top+1)) pc += GETARG_S(i);
        break;

      case OP_JMPLT:
        top -= 2;
        if (luaV_lessthan(L, top, top+1, top+2)) pc += GETARG_S(i);
        break;

      case OP_JMPLE:  /* a <= b  ===  !(b<a) */
        top -= 2;
        if (!luaV_lessthan(L, top+1, top, top+2)) pc += GETARG_S(i);
        break;

      case OP_JMPGT:  /* a > b  ===  (b<a) */
        top -= 2;
        if (luaV_lessthan(L, top+1, top, top+2)) pc += GETARG_S(i);
        break;

      case OP_JMPGE:  /* a >= b  ===  !(a<b) */
        top -= 2;
        if (!luaV_lessthan(L, top, top+1, top+2)) pc += GETARG_S(i);
        break;

      case OP_JMPT:
        if (ttype(--top) != TAG_NIL) pc += GETARG_S(i);
        break;

      case OP_JMPF:
        if (ttype(--top) == TAG_NIL) pc += GETARG_S(i);
        break;

      case OP_JMPONT:
        if (ttype(top-1) != TAG_NIL) pc += GETARG_S(i);
        else top--;
        break;

      case OP_JMPONF:
        if (ttype(top-1) == TAG_NIL) pc += GETARG_S(i);
        else top--;
        break;

      case OP_JMP:
        pc += GETARG_S(i);
        break;

      case OP_PUSHNILJMP:
        ttype(top++) = TAG_NIL;
        pc++;
        break;

      case OP_FORPREP:
        if (tonumber(top-1))
          lua_error(L, "`for' step must be a number");
        if (tonumber(top-2))
          lua_error(L, "`for' limit must be a number");
        if (tonumber(top-3))
          lua_error(L, "`for' initial value must be a number");
        if (nvalue(top-1) > 0 ?
            nvalue(top-3) > nvalue(top-2) :
            nvalue(top-3) < nvalue(top-2)) {  /* `empty' loop? */
          top -= 3;  /* remove control variables */
          pc += GETARG_S(i)+1;  /* jump to loop end */
        }
        break;

      case OP_FORLOOP: {
        LUA_ASSERT(L, ttype(top-1) == TAG_NUMBER, "invalid step");
        LUA_ASSERT(L, ttype(top-2) == TAG_NUMBER, "invalid limit");
        if (ttype(top-3) != TAG_NUMBER)
          lua_error(L, "`for' index must be a number");
        nvalue(top-3) += nvalue(top-1);  /* increment index */
        if (nvalue(top-1) > 0 ?
            nvalue(top-3) > nvalue(top-2) :
            nvalue(top-3) < nvalue(top-2))
          top -= 3;  /* end loop: remove control variables */
        else
          pc += GETARG_S(i);  /* repeat loop */
        break;
      }

      case OP_LFORPREP: {
        if (ttype(top-1) != TAG_TABLE)
          lua_error(L, "`for' table must be a table");
        top++;  /* counter */
        L->top = top;
        ttype(top-1) = TAG_NUMBER;
        nvalue(top-1) = (Number)luaA_next(L, hvalue(top-2), 0);  /* counter */
        if (nvalue(top-1) == 0) {  /* `empty' loop? */
          top -= 2;  /* remove table and counter */
          pc += GETARG_S(i)+1;  /* jump to loop end */
        }
        else {
          top += 2;  /* index,value */
          LUA_ASSERT(L, top==L->top, "bad top");
        }
        break;
      }

      case OP_LFORLOOP: {
        int n;
        top -= 2;  /* remove old index,value */
        LUA_ASSERT(L, ttype(top-2) == TAG_TABLE, "invalid table");
        LUA_ASSERT(L, ttype(top-1) == TAG_NUMBER, "invalid counter");
        L->top = top;
        n = luaA_next(L, hvalue(top-2), (int)nvalue(top-1));
        if (n == 0)  /* end loop? */
          top -= 2;  /* remove table and counter */
        else {
          nvalue(top-1) = (Number)n;
          top += 2;  /* new index,value */
          pc += GETARG_S(i);  /* repeat loop */
        }
        break;
      }

      case OP_CLOSURE:
        L->top = top;
        luaV_Lclosure(L, tf->kproto[GETARG_A(i)], GETARG_B(i));
        top = L->top;
        luaC_checkGC(L);
        break;

    }
  }
}
