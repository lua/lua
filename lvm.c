/*
** $Id: lvm.c,v 1.95 2000/03/10 18:37:44 roberto Exp roberto $
** Lua virtual machine
** See Copyright Notice in lua.h
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LUA_REENTRANT

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


void luaV_setn (lua_State *L, Hash *t, int val) {
  TObject index, value;
  ttype(&index) = TAG_STRING; tsvalue(&index) = luaS_new(L, "n");
  ttype(&value) = TAG_NUMBER; nvalue(&value) = val;
  luaH_set(L, t, &index, &value);
}


void luaV_closure (lua_State *L, int nelems) {
  if (nelems > 0) {
    Closure *c = luaF_newclosure(L, nelems);
    c->consts[0] = *(L->top-1);
    L->top -= nelems;
    while (nelems--)
      c->consts[nelems+1] = *(L->top-1+nelems);
    ttype(L->top-1) = (ttype(&c->consts[0]) == TAG_CPROTO) ?
                        TAG_CCLOSURE : TAG_LCLOSURE;
    (L->top-1)->value.cl = c;
  }
}


/*
** Function to index a table.
** Receives the table at top-2 and the index at top-1.
*/
void luaV_gettable (lua_State *L, StkId top) {
  TObject *table = top-2;
  const TObject *im;
  if (ttype(table) != TAG_ARRAY) {  /* not a table, get gettable TM */
    im = luaT_getimbyObj(L, table, IM_GETTABLE);
    if (ttype(im) == TAG_NIL) {
      L->top = top;
      luaG_indexerror(L, table);
    }
  }
  else {  /* object is a table... */
    int tg = table->value.a->htag;
    im = luaT_getim(L, tg, IM_GETTABLE);
    if (ttype(im) == TAG_NIL) {  /* and does not have a `gettable' TM */
      const TObject *h = luaH_get(L, avalue(table), table+1);
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
  if (ttype(t) != TAG_ARRAY) {  /* not a table, get `settable' method */
    L->top = top;
    im = luaT_getimbyObj(L, t, IM_SETTABLE);
    if (ttype(im) == TAG_NIL)
      luaG_indexerror(L, t);
  }
  else {  /* object is a table... */
    im = luaT_getim(L, avalue(t)->htag, IM_SETTABLE);
    if (ttype(im) == TAG_NIL) {  /* and does not have a `settable' method */
      luaH_set(L, avalue(t), t+1, top-1);
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


void luaV_rawsettable (lua_State *L, StkId t) {
  if (ttype(t) != TAG_ARRAY)
    lua_error(L, "indexed expression not a table");
  else {
    luaH_set(L, avalue(t), t+1, L->top-1);
    L->top -= 3;
  }
}


void luaV_getglobal (lua_State *L, GlobalVar *gv, StkId top) {
  const TObject *value = &gv->value;
  TObject *im = luaT_getimbyObj(L, value, IM_GETGLOBAL);
  if (ttype(im) == TAG_NIL)  /* is there a tag method? */
    *top = *value;  /* default behavior */
  else {  /* tag method */
    luaD_checkstack(L, 3);
    *top = *im;
    ttype(top+1) = TAG_STRING;
    tsvalue(top+1) = gv->name;  /* global name */
    *(top+2) = *value;
    L->top = top+3;
    luaD_call(L, top, 1);
  }
}


void luaV_setglobal (lua_State *L, GlobalVar *gv, StkId top) {
  const TObject *oldvalue = &gv->value;
  const TObject *im = luaT_getimbyObj(L, oldvalue, IM_SETGLOBAL);
  if (ttype(im) == TAG_NIL)  /* is there a tag method? */
    gv->value = *(top-1);
  else {
    luaD_checkstack(L, 3);
    *(top+2) = *(top-1);  /* new value */
    *(top+1) = *oldvalue;
    ttype(top) = TAG_STRING;
    tsvalue(top) = gv->name;
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
  long ll = ls->u.s.len;
  const char *r = rs->str;
  long lr = rs->u.s.len;
  for (;;) {
    long temp = strcoll(l, r);
    if (temp != 0) return temp;
    /* strings are equal up to a '\0' */
    temp = strlen(l);  /* index of first '\0' in both strings */
    if (temp == ll)  /* l is finished? */
      return (temp == lr) ? 0 : -1;  /* l is equal or smaller than r */
    else if (temp == lr)  /* r is finished? */
      return 1;  /* l is greater than r (because l is not finished) */
    /* both strings longer than temp; go on comparing (after the '\0') */
    temp++;
    l += temp; ll -= temp; r += temp; lr -= temp;
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
      long tl = tsvalue(top-1)->u.s.len + tsvalue(top-2)->u.s.len;
      char *buffer;
      int i;
      while (n < total && !tostring(L, top-n-1)) {  /* collect total length */
        tl += tsvalue(top-n-1)->u.s.len;
        n++;
      }
      buffer = luaL_openspace(L, tl);
      tl = 0;
      for (i=n; i>0; i--) {  /* concat all strings */
        long l = tsvalue(top-i)->u.s.len;
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
  htab = avalue(tab) = luaH_new(L, nvararg+1);  /* +1 for field `n' */
  ttype(tab) = TAG_ARRAY;
  for (i=0; i<nvararg; i++)
    luaH_setint(L, htab, i+1, firstelem+i);
  luaV_setn(L, htab, nvararg);  /* store counter in field `n' */
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
StkId luaV_execute (lua_State *L, const Closure *cl, const Proto *tf,
                    register StkId base) {
  register StkId top;  /* keep top local, for performance */
  register const Instruction *pc = tf->code;
  TString **kstr = tf->kstr;
  if (L->callhook)
    luaD_callHook(L, base-1, L->callhook, "call");
  luaD_checkstack(L, tf->maxstacksize+EXTRA_STACK);
  if (tf->is_vararg) {  /* varargs? */
    adjust_varargs(L, base, tf->numparams);
    luaC_checkGC(L);
  }
  else
    luaD_adjusttop(L, base, tf->numparams);
  top = L->top;
  for (;;) {
    register Instruction i = *pc++;
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
        *top++ = cl->consts[GETARG_U(i)+1];
        break;

      case OP_PUSHLOCAL:
        *top++ = *(base+GETARG_U(i));
        break;

      case OP_GETGLOBAL:
        luaV_getglobal(L, kstr[GETARG_U(i)]->u.s.gv, top);
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
        avalue(top) = luaH_new(L, GETARG_U(i));
        ttype(top) = TAG_ARRAY;
        top++;
        break;

      case OP_SETLOCAL:
        *(base+GETARG_U(i)) = *(--top);
        break;

      case OP_SETGLOBAL:
        luaV_setglobal(L, kstr[GETARG_U(i)]->u.s.gv, top);
        top--;
        break;

      case OP_SETTABLEPOP:
        luaV_settable(L, top-3, top);
        top -= 3;  /* pop table, index, and value */
        break;

      case OP_SETTABLE:
        luaV_settable(L, top-3-GETARG_U(i), top);
        top--;  /* pop value */
        break;

      case OP_SETLIST: {
        int aux = GETARG_A(i) * LFIELDS_PER_FLUSH;
        int n = GETARG_B(i)+1;
        Hash *arr = avalue(top-n-1);
        L->top = top-n;  /* final value of `top' (in case of errors) */
        for (; n; n--)
          luaH_setint(L, arr, n+aux, --top);
        break;
      }

      case OP_SETMAP: {
        int n = GETARG_U(i);
        StkId finaltop = top-2*(n+1);
        Hash *arr = avalue(finaltop-1);
        L->top = finaltop;  /* final value of `top' (in case of errors) */
        do {
          luaH_set(L, arr, top-2, top-1);
          top-=2;
        } while (n--);
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

      case OP_CONC: {
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

      case OP_IFNEQJMP:
        top -= 2;
        if (!luaO_equalObj(top, top+1)) pc += GETARG_S(i);
        break;

      case OP_IFEQJMP:
        top -= 2;
        if (luaO_equalObj(top, top+1)) pc += GETARG_S(i);
        break;

      case OP_IFLTJMP:
        top -= 2;
        if (luaV_lessthan(L, top, top+1, top+2)) pc += GETARG_S(i);
        break;

      case OP_IFLEJMP:  /* a <= b  ===  !(b<a) */
        top -= 2;
        if (!luaV_lessthan(L, top+1, top, top+2)) pc += GETARG_S(i);
        break;

      case OP_IFGTJMP:  /* a > b  ===  (b<a) */
        top -= 2;
        if (luaV_lessthan(L, top+1, top, top+2)) pc += GETARG_S(i);
        break;

      case OP_IFGEJMP:  /* a >= b  ===  !(a<b) */
        top -= 2;
        if (!luaV_lessthan(L, top, top+1, top+2)) pc += GETARG_S(i);
        break;

      case OP_IFTJMP:
        if (ttype(--top) != TAG_NIL) pc += GETARG_S(i);
        break;

      case OP_IFFJMP:
        if (ttype(--top) == TAG_NIL) pc += GETARG_S(i);
        break;

      case OP_ONTJMP:
        if (ttype(top-1) != TAG_NIL) pc += GETARG_S(i);
        else top--;
        break;

      case OP_ONFJMP:
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

      case OP_CLOSURE:
        ttype(top) = TAG_LPROTO;
        tfvalue(top) = tf->kproto[GETARG_A(i)];
        L->top = ++top;
        luaV_closure(L, GETARG_B(i));
        top -= GETARG_B(i);
        luaC_checkGC(L);
        break;

      case OP_SETLINE:
        if ((base-1)->ttype != TAG_LINE) {
          /* open space for LINE value */
          int n = top-base;
          while (n--) base[n+1] = base[n];
          base++;
          top++;
          (base-1)->ttype = TAG_LINE;
        }
        (base-1)->value.i = GETARG_U(i);
        if (L->linehook) {
          L->top = top;
          luaD_lineHook(L, base-2, GETARG_U(i));
        }
        break;

    }
  }
}
