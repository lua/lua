/*
** $Id: lvm.c,v 1.156 2001/01/24 15:45:33 roberto Exp roberto $
** Lua virtual machine
** See Copyright Notice in lua.h
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"

#include "lapi.h"
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
#define	EXTRA_FSTACK	8



int luaV_tonumber (TObject *obj) {
  if (ttype(obj) != LUA_TSTRING)
    return 1;
  else {
    if (!luaO_str2d(svalue(obj), &nvalue(obj)))
      return 2;
    ttype(obj) = LUA_TNUMBER;
    return 0;
  }
}


int luaV_tostring (lua_State *L, TObject *obj) {  /* LUA_NUMBER */
  if (ttype(obj) != LUA_TNUMBER)
    return 1;
  else {
    char s[32];  /* 16 digits, sign, point and \0  (+ some extra...) */
    lua_number2str(s, nvalue(obj));  /* convert `s' to number */
    setsvalue(obj, luaS_new(L, s));
    return 0;
  }
}


static void traceexec (lua_State *L, StkId base, StkId top, lua_Hook linehook) {
  CallInfo *ci = infovalue(base-1);
  int *lineinfo = ci->func->f.l->lineinfo;
  int pc = (*ci->pc - ci->func->f.l->code) - 1;
  int newline;
  if (pc == 0) {  /* may be first time? */
    ci->line = 1;
    ci->refi = 0;
    ci->lastpc = pc+1;  /* make sure it will call linehook */
  }
  newline = luaG_getline(lineinfo, pc, ci->line, &ci->refi);
  /* calls linehook when enters a new line or jumps back (loop) */
  if (newline != ci->line || pc <= ci->lastpc) {
    ci->line = newline;
    L->top = top;
    luaD_lineHook(L, base-2, newline, linehook);
  }
  ci->lastpc = pc;
}


static Closure *luaV_closure (lua_State *L, int nelems) {
  Closure *c = luaF_newclosure(L, nelems);
  L->top -= nelems;
  while (nelems--)
    setobj(&c->upvalue[nelems], L->top+nelems);
  setclvalue(L->top, c);
  incr_top;
  return c;
}


void luaV_Cclosure (lua_State *L, lua_CFunction c, int nelems) {
  Closure *cl = luaV_closure(L, nelems);
  cl->f.c = c;
  cl->isC = 1;
}


void luaV_Lclosure (lua_State *L, Proto *l, int nelems) {
  Closure *cl = luaV_closure(L, nelems);
  cl->f.l = l;
  cl->isC = 0;
}


/*
** Function to index a table.
** Receives the table at `t' and the key at top.
*/
const TObject *luaV_gettable (lua_State *L, StkId t) {
  Closure *tm;
  int tg;
  if (ttype(t) == LUA_TTABLE &&  /* `t' is a table? */
      ((tg = hvalue(t)->htag) == LUA_TTABLE ||  /* with default tag? */
        luaT_gettm(G(L), tg, TM_GETTABLE) == NULL)) { /* or no TM? */
    /* do a primitive get */
    const TObject *h = luaH_get(hvalue(t), L->top-1);
    /* result is no nil or there is no `index' tag method? */
    if (ttype(h) != LUA_TNIL || ((tm=luaT_gettm(G(L), tg, TM_INDEX)) == NULL))
      return h;  /* return result */
    /* else call `index' tag method */
  }
  else {  /* try a `gettable' tag method */
    tm = luaT_gettmbyObj(G(L), t, TM_GETTABLE);
  }
  if (tm != NULL) {  /* is there a tag method? */
    luaD_checkstack(L, 2);
    setobj(L->top+1, L->top-1);  /* key */
    setobj(L->top, t);  /* table */
    setclvalue(L->top-1, tm);  /* tag method */
    L->top += 2;
    luaD_call(L, L->top - 3, 1);
    return L->top - 1;  /* call result */
  }
  else {  /* no tag method */
    luaG_typeerror(L, t, "index");
    return NULL;  /* to avoid warnings */
  }
}


/*
** Receives table at `t', key at `key' and value at top.
*/
void luaV_settable (lua_State *L, StkId t, StkId key) {
  int tg;
  if (ttype(t) == LUA_TTABLE &&  /* `t' is a table? */
      ((tg = hvalue(t)->htag) == LUA_TTABLE ||  /* with default tag? */
        luaT_gettm(G(L), tg, TM_SETTABLE) == NULL)) { /* or no TM? */
    setobj(luaH_set(L, hvalue(t), key), L->top-1);  /* do a primitive set */
  }
  else {  /* try a `settable' tag method */
    Closure *tm = luaT_gettmbyObj(G(L), t, TM_SETTABLE);
    if (tm != NULL) {
      luaD_checkstack(L, 3);
      setobj(L->top+2, L->top-1);
      setobj(L->top+1, key);
      setobj(L->top, t);
      setclvalue(L->top-1, tm);
      L->top += 3;
      luaD_call(L, L->top - 4, 0);  /* call `settable' tag method */
    }
    else  /* no tag method... */
      luaG_typeerror(L, t, "index");
  }
}


const TObject *luaV_getglobal (lua_State *L, TString *s) {
  const TObject *value = luaH_getstr(L->gt, s);
  Closure *tm;
  if (!HAS_TM_GETGLOBAL(L, ttype(value)) ||  /* is there a tag method? */
      (tm = luaT_gettmbyObj(G(L), value, TM_GETGLOBAL)) == NULL)
    return value;  /* default behavior */
  else {  /* tag method */
    luaD_checkstack(L, 3);
    setclvalue(L->top, tm);
    setsvalue(L->top+1, s);  /* global name */
    setobj(L->top+2, value);
    L->top += 3;
    luaD_call(L, L->top - 3, 1);
    return L->top - 1;
  }
}


void luaV_setglobal (lua_State *L, TString *s) {
  TObject *oldvalue = luaH_setstr(L, L->gt, s);
  Closure *tm;
  if (!HAS_TM_SETGLOBAL(L, ttype(oldvalue)) ||  /* no tag methods? */
     (tm = luaT_gettmbyObj(G(L), oldvalue, TM_SETGLOBAL)) == NULL) {
    setobj(oldvalue, L->top - 1);  /* raw set */
  }
  else {  /* call tag method */
    luaD_checkstack(L, 3);
    setobj(L->top+2, L->top-1);  /* new value */
    setobj(L->top+1, oldvalue);  /* old value */
    setsvalue(L->top, s);        /* var name */
    setclvalue(L->top-1, tm);    /* tag method */
    L->top += 3;
    luaD_call(L, L->top - 4, 0);
  }
}


static int call_binTM (lua_State *L, StkId top, TMS event) {
  /* try first operand */
  Closure *tm = luaT_gettmbyObj(G(L), top-2, event);
  L->top = top;
  if (tm == NULL) {
    tm = luaT_gettmbyObj(G(L), top-1, event);  /* try second operand */
    if (tm == NULL) {
      tm = luaT_gettm(G(L), 0, event);  /* try a `global' method */
      if (tm == NULL)
        return 0;  /* error */
    }
  }
  setsvalue(L->top, luaS_new(L, luaT_eventname[event]));
  incr_top;
  luaD_callTM(L, tm, 3, 1);
  return 1;
}


static void call_arith (lua_State *L, StkId top, TMS event) {
  if (!call_binTM(L, top, event))
    luaG_binerror(L, top-2, LUA_TNUMBER, "perform arithmetic on");
}


static int luaV_strlessthan (const TString *ls, const TString *rs) {
  const char *l = ls->str;
  size_t ll = ls->len;
  const char *r = rs->str;
  size_t lr = rs->len;
  for (;;) {
    int temp = strcoll(l, r);
    if (temp != 0) return (temp < 0);
    else {  /* strings are equal up to a '\0' */
      size_t len = strlen(l);  /* index of first '\0' in both strings */
      if (len == lr)  /* r is finished? */
        return 0;  /* l is equal or greater than r */
      else if (len == ll)  /* l is finished? */
        return 1;  /* l is smaller than r (because r is not finished) */
      /* both strings longer than `len'; go on comparing (after the '\0') */
      len++;
      l += len; ll -= len; r += len; lr -= len;
    }
  }
}


int luaV_lessthan (lua_State *L, const TObject *l, const TObject *r, StkId top) {
  if (ttype(l) == LUA_TNUMBER && ttype(r) == LUA_TNUMBER)
    return (nvalue(l) < nvalue(r));
  else if (ttype(l) == LUA_TSTRING && ttype(r) == LUA_TSTRING)
    return luaV_strlessthan(tsvalue(l), tsvalue(r));
  else {  /* call TM */
    luaD_checkstack(L, 2);
    setobj(top++, l);
    setobj(top++, r);
    if (!call_binTM(L, top, TM_LT))
      luaG_ordererror(L, top-2);
    L->top--;
    return (ttype(L->top) != LUA_TNIL);
  }
}


void luaV_strconc (lua_State *L, int total, StkId top) {
  do {
    int n = 2;  /* number of elements handled in this pass (at least 2) */
    if (tostring(L, top-2) || tostring(L, top-1)) {
      if (!call_binTM(L, top, TM_CONCAT))
        luaG_binerror(L, top-2, LUA_TSTRING, "concat");
    }
    else if (tsvalue(top-1)->len > 0) {  /* if len=0, do nothing */
      /* at least two string values; get as many as possible */
      luint32 tl = (luint32)tsvalue(top-1)->len + 
                   (luint32)tsvalue(top-2)->len;
      char *buffer;
      int i;
      while (n < total && !tostring(L, top-n-1)) {  /* collect total length */
        tl += tsvalue(top-n-1)->len;
        n++;
      }
      if (tl > MAX_SIZET) luaD_error(L, "string size overflow");
      buffer = luaO_openspace(L, tl);
      tl = 0;
      for (i=n; i>0; i--) {  /* concat all strings */
        size_t l = tsvalue(top-i)->len;
        memcpy(buffer+tl, tsvalue(top-i)->str, l);
        tl += l;
      }
      setsvalue(top-n, luaS_newlstr(L, buffer, tl));
    }
    total -= n-1;  /* got `n' strings to create 1 new */
    top -= n-1;
  } while (total > 1);  /* repeat until only 1 result left */
}


static void luaV_pack (lua_State *L, StkId firstelem) {
  int i;
  Hash *htab = luaH_new(L, 0);
  for (i=0; firstelem+i<L->top; i++)
    setobj(luaH_setnum(L, htab, i+1), firstelem+i);
  /* store counter in field `n' */
  setnvalue(luaH_setstr(L, htab, luaS_newliteral(L, "n")), i);
  L->top = firstelem;  /* remove elements from the stack */
  sethvalue(L->top, htab);
  incr_top;
}


static void adjust_varargs (lua_State *L, StkId base, int nfixargs) {
  int nvararg = (L->top-base) - nfixargs;
  if (nvararg < 0)
    luaD_adjusttop(L, base, nfixargs);
  luaV_pack(L, base+nfixargs);
}



#define dojump(pc, i)	{ int d = GETARG_S(i); pc += d; }

/*
** Executes the given Lua function. Parameters are between [base,top).
** Returns n such that the the results are between [n,top).
*/
StkId luaV_execute (lua_State *L, const Closure *cl, StkId base) {
  const Proto *const tf = cl->f.l;
  StkId top;  /* keep top local, for performance */
  const Instruction *pc = tf->code;
  TString **const kstr = tf->kstr;
  const lua_Hook linehook = L->linehook;
  infovalue(base-1)->pc = &pc;
  luaD_checkstack(L, tf->maxstacksize+EXTRA_FSTACK);
  if (tf->is_vararg)  /* varargs? */
    adjust_varargs(L, base, tf->numparams);
  else
    luaD_adjusttop(L, base, tf->numparams);
  top = L->top;
  /* main loop of interpreter */
  for (;;) {
    const Instruction i = *pc++;
    if (linehook)
      traceexec(L, base, top, linehook);
    switch (GET_OPCODE(i)) {
      case OP_RETURN: {
        L->top = top;
        return base+GETARG_U(i);
      }
      case OP_CALL: {
        int nres = GETARG_B(i);
        if (nres == MULT_RET) nres = LUA_MULTRET;
        L->top = top;
        luaD_call(L, base+GETARG_A(i), nres);
        top = L->top;
        break;
      }
      case OP_TAILCALL: {
        L->top = top;
        luaD_call(L, base+GETARG_A(i), LUA_MULTRET);
        return base+GETARG_B(i);
      }
      case OP_PUSHNIL: {
        int n = GETARG_U(i);
        lua_assert(n>0);
        do {
          setnilvalue(top++);
        } while (--n > 0);
        break;
      }
      case OP_POP: {
        top -= GETARG_U(i);
        break;
      }
      case OP_PUSHINT: {
        setnvalue(top++, (lua_Number)GETARG_S(i));
        break;
      }
      case OP_PUSHSTRING: {
        setsvalue(top++, kstr[GETARG_U(i)]);
        break;
      }
      case OP_PUSHNUM: {
        setnvalue(top++, tf->knum[GETARG_U(i)]);
        break;
      }
      case OP_PUSHNEGNUM: {
        setnvalue(top++, -tf->knum[GETARG_U(i)]);
        break;
      }
      case OP_PUSHUPVALUE: {
        setobj(top++, &cl->upvalue[GETARG_U(i)]);
        break;
      }
      case OP_GETLOCAL: {
        setobj(top++, base+GETARG_U(i));
        break;
      }
      case OP_GETGLOBAL: {
        L->top = top;
        setobj(top++, luaV_getglobal(L, kstr[GETARG_U(i)]));
        break;
      }
      case OP_GETTABLE: {
        L->top = top;
        top--;
        setobj(top-1, luaV_gettable(L, top-1));
        break;
      }
      case OP_GETDOTTED: {
        setsvalue(top, kstr[GETARG_U(i)]);
        L->top = top+1;
        setobj(top-1, luaV_gettable(L, top-1));
        break;
      }
      case OP_GETINDEXED: {
        setobj(top, base+GETARG_U(i));
        L->top = top+1;
        setobj(top-1, luaV_gettable(L, top-1));
        break;
      }
      case OP_PUSHSELF: {
        TObject receiver;
        setobj(&receiver, top-1);
        setsvalue(top++, kstr[GETARG_U(i)]);
        L->top = top;
        setobj(top-2, luaV_gettable(L, top-2));
        setobj(top-1, &receiver);
        break;
      }
      case OP_CREATETABLE: {
        L->top = top;
        luaC_checkGC(L);
        sethvalue(top++, luaH_new(L, GETARG_U(i)));
        break;
      }
      case OP_SETLOCAL: {
        setobj(base+GETARG_U(i), --top);
        break;
      }
      case OP_SETGLOBAL: {
        L->top = top--;
        luaV_setglobal(L, kstr[GETARG_U(i)]);
        break;
      }
      case OP_SETTABLE: {
        StkId t = top-GETARG_A(i);
        L->top = top;
        luaV_settable(L, t, t+1);
        top -= GETARG_B(i);  /* pop values */
        break;
      }
      case OP_SETLIST: {
        int aux = GETARG_A(i) * LFIELDS_PER_FLUSH;
        int n = GETARG_B(i);
        Hash *arr = hvalue(top-n-1);
        L->top = top-n;  /* final value of `top' (in case of errors) */
        for (; n; n--)
          setobj(luaH_setnum(L, arr, n+aux), --top);
        break;
      }
      case OP_SETMAP: {
        int n = GETARG_U(i);
        StkId finaltop = top-2*n;
        Hash *arr = hvalue(finaltop-1);
        L->top = finaltop;  /* final value of `top' (in case of errors) */
        for (; n; n--) {
          top-=2;
          setobj(luaH_set(L, arr, top), top+1);
        }
        break;
      }
      case OP_ADD: {
        if (tonumber(top-2) || tonumber(top-1))
          call_arith(L, top, TM_ADD);
        else
          nvalue(top-2) += nvalue(top-1);
        top--;
        break;
      }
      case OP_ADDI: {
        if (tonumber(top-1)) {
          setnvalue(top, (lua_Number)GETARG_S(i));
          call_arith(L, top+1, TM_ADD);
        }
        else
          nvalue(top-1) += (lua_Number)GETARG_S(i);
        break;
      }
      case OP_SUB: {
        if (tonumber(top-2) || tonumber(top-1))
          call_arith(L, top, TM_SUB);
        else
          nvalue(top-2) -= nvalue(top-1);
        top--;
        break;
      }
      case OP_MULT: {
        if (tonumber(top-2) || tonumber(top-1))
          call_arith(L, top, TM_MUL);
        else
          nvalue(top-2) *= nvalue(top-1);
        top--;
        break;
      }
      case OP_DIV: {
        if (tonumber(top-2) || tonumber(top-1))
          call_arith(L, top, TM_DIV);
        else
          nvalue(top-2) /= nvalue(top-1);
        top--;
        break;
      }
      case OP_POW: {
        if (!call_binTM(L, top, TM_POW))
          luaD_error(L, "undefined operation");
        top--;
        break;
      }
      case OP_CONCAT: {
        int n = GETARG_U(i);
        luaV_strconc(L, n, top);
        top -= n-1;
        L->top = top;
        luaC_checkGC(L);
        break;
      }
      case OP_MINUS: {
        if (tonumber(top-1)) {
          setnilvalue(top);
          call_arith(L, top+1, TM_UNM);
        }
        else
          nvalue(top-1) = -nvalue(top-1);
        break;
      }
      case OP_NOT: {
        ttype(top-1) =
           (ttype(top-1) == LUA_TNIL) ? LUA_TNUMBER : LUA_TNIL;
        nvalue(top-1) = 1;
        break;
      }
      case OP_JMPNE: {
        top -= 2;
        if (!luaO_equalObj(top, top+1)) dojump(pc, i);
        break;
      }
      case OP_JMPEQ: {
        top -= 2;
        if (luaO_equalObj(top, top+1)) dojump(pc, i);
        break;
      }
      case OP_JMPLT: {
        top -= 2;
        if (luaV_lessthan(L, top, top+1, top+2)) dojump(pc, i);
        break;
      }
      case OP_JMPLE: {  /* a <= b  ===  !(b<a) */
        top -= 2;
        if (!luaV_lessthan(L, top+1, top, top+2)) dojump(pc, i);
        break;
      }
      case OP_JMPGT: {  /* a > b  ===  (b<a) */
        top -= 2;
        if (luaV_lessthan(L, top+1, top, top+2)) dojump(pc, i);
        break;
      }
      case OP_JMPGE: {  /* a >= b  ===  !(a<b) */
        top -= 2;
        if (!luaV_lessthan(L, top, top+1, top+2)) dojump(pc, i);
        break;
      }
      case OP_JMPT: {
        if (ttype(--top) != LUA_TNIL) dojump(pc, i);
        break;
      }
      case OP_JMPF: {
        if (ttype(--top) == LUA_TNIL) dojump(pc, i);
        break;
      }
      case OP_JMPONT: {
        if (ttype(top-1) == LUA_TNIL) top--;
        else dojump(pc, i);
        break;
      }
      case OP_JMPONF: {
        if (ttype(top-1) != LUA_TNIL) top--;
        else dojump(pc, i);
        break;
      }
      case OP_JMP: {
        dojump(pc, i);
        break;
      }
      case OP_PUSHNILJMP: {
        setnilvalue(top++);
        pc++;
        break;
      }
      case OP_FORPREP: {
        if (tonumber(top-1))
          luaD_error(L, "`for' step must be a number");
        if (tonumber(top-2))
          luaD_error(L, "`for' limit must be a number");
        if (tonumber(top-3))
          luaD_error(L, "`for' initial value must be a number");
        if (nvalue(top-1) > 0 ?
            nvalue(top-3) > nvalue(top-2) :
            nvalue(top-3) < nvalue(top-2)) {  /* `empty' loop? */
          top -= 3;  /* remove control variables */
          dojump(pc, i);  /* jump to loop end */
        }
        break;
      }
      case OP_FORLOOP: {
        lua_assert(ttype(top-1) == LUA_TNUMBER);
        lua_assert(ttype(top-2) == LUA_TNUMBER);
        if (ttype(top-3) != LUA_TNUMBER)
          luaD_error(L, "`for' index must be a number");
        nvalue(top-3) += nvalue(top-1);  /* increment index */
        if (nvalue(top-1) > 0 ?
            nvalue(top-3) > nvalue(top-2) :
            nvalue(top-3) < nvalue(top-2))
          top -= 3;  /* end loop: remove control variables */
        else
          dojump(pc, i);  /* repeat loop */
        break;
      }
      case OP_LFORPREP: {
        Node *node;
        if (ttype(top-1) != LUA_TTABLE)
          luaD_error(L, "`for' table must be a table");
        node = luaH_next(L, hvalue(top-1), &luaO_nilobject);
        if (node == NULL) {  /* `empty' loop? */
          top--;  /* remove table */
          dojump(pc, i);  /* jump to loop end */
        }
        else {
          top += 2;  /* index,value */
          setobj(top-2, key(node));
          setobj(top-1, val(node));
        }
        break;
      }
      case OP_LFORLOOP: {
        Node *node;
        lua_assert(ttype(top-3) == LUA_TTABLE);
        node = luaH_next(L, hvalue(top-3), top-2);
        if (node == NULL)  /* end loop? */
          top -= 3;  /* remove table, key, and value */
        else {
          setobj(top-2, key(node));
          setobj(top-1, val(node));
          dojump(pc, i);  /* repeat loop */
        }
        break;
      }
      case OP_CLOSURE: {
        L->top = top;
        luaV_Lclosure(L, tf->kproto[GETARG_A(i)], GETARG_B(i));
        top = L->top;
        luaC_checkGC(L);
        break;
      }
    }
  }
}
