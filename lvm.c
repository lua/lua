/*
** $Id: lvm.c,v 1.86 2000/02/11 16:52:54 roberto Exp roberto $
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


#define highbyte(L, x)	((x)<<8)


/*
** Extra stack size to run a function:
** LUA_T_LINE(1), NAME(1), TM calls(3) (plus some extra...)
*/
#define	EXTRA_STACK	8



static TaggedString *strconc (lua_State *L, const TaggedString *l,
                                            const TaggedString *r) {
  long nl = l->u.s.len;
  long nr = r->u.s.len;
  char *buffer = luaL_openspace(L, nl+nr);
  memcpy(buffer, l->str, nl);
  memcpy(buffer+nl, r->str, nr);
  return luaS_newlstr(L, buffer, nl+nr);
}


int luaV_tonumber (TObject *obj) {  /* LUA_NUMBER */
  if (ttype(obj) != LUA_T_STRING)
    return 1;
  else {
    if (!luaO_str2d(svalue(obj), &nvalue(obj)))
      return 2;
    ttype(obj) = LUA_T_NUMBER;
    return 0;
  }
}


int luaV_tostring (lua_State *L, TObject *obj) {  /* LUA_NUMBER */
  if (ttype(obj) != LUA_T_NUMBER)
    return 1;
  else {
    char s[32];  /* 16 digits, sign, point and \0  (+ some extra...) */
    sprintf(s, "%.16g", (double)nvalue(obj));
    tsvalue(obj) = luaS_new(L, s);
    ttype(obj) = LUA_T_STRING;
    return 0;
  }
}


void luaV_setn (lua_State *L, Hash *t, int val) {
  TObject index, value;
  ttype(&index) = LUA_T_STRING; tsvalue(&index) = luaS_new(L, "n");
  ttype(&value) = LUA_T_NUMBER; nvalue(&value) = val;
  luaH_set(L, t, &index, &value);
}


void luaV_closure (lua_State *L, int nelems) {
  if (nelems > 0) {
    Closure *c = luaF_newclosure(L, nelems);
    c->consts[0] = *(L->top-1);
    L->top -= nelems;
    while (nelems--)
      c->consts[nelems+1] = *(L->top-1+nelems);
    ttype(L->top-1) = (ttype(&c->consts[0]) == LUA_T_CPROTO) ?
                        LUA_T_CCLOSURE : LUA_T_LCLOSURE;
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
  if (ttype(table) != LUA_T_ARRAY) {  /* not a table, get gettable method */
    im = luaT_getimbyObj(L, table, IM_GETTABLE);
    if (ttype(im) == LUA_T_NIL) {
      L->top = top;
      luaG_indexerror(L, table);
    }
  }
  else {  /* object is a table... */
    int tg = table->value.a->htag;
    im = luaT_getim(L, tg, IM_GETTABLE);
    if (ttype(im) == LUA_T_NIL) {  /* and does not have a `gettable' method */
      const TObject *h = luaH_get(L, avalue(table), table+1);
      if (ttype(h) == LUA_T_NIL &&
          (ttype(im=luaT_getim(L, tg, IM_INDEX)) != LUA_T_NIL)) {
        /* result is nil and there is an `index' tag method */
        L->top = top;
        luaD_callTM(L, im, 2, 1);  /* calls it */
      }
      else
        *table = *h;  /* `push' result into table position */
      return;
    }
    /* else it has a `gettable' method, go through to next command */
  }
  /* object is not a table, or it has a `gettable' method */
  L->top = top;
  luaD_callTM(L, im, 2, 1);
}


/*
** Receives table at *t, index at *(t+1) and value at `top'.
** WARNING: caller must assure 3 extra stack slots (to call a tag method)
*/
void luaV_settable (lua_State *L, StkId t, StkId top) {
  const TObject *im;
  if (ttype(t) != LUA_T_ARRAY) {  /* not a table, get `settable' method */
    L->top = top;
    im = luaT_getimbyObj(L, t, IM_SETTABLE);
    if (ttype(im) == LUA_T_NIL)
      luaG_indexerror(L, t);
  }
  else {  /* object is a table... */
    im = luaT_getim(L, avalue(t)->htag, IM_SETTABLE);
    if (ttype(im) == LUA_T_NIL) {  /* and does not have a `settable' method */
      luaH_set(L, avalue(t), t+1, top-1);
      return;
    }
    /* else it has a `settable' method, go through to next command */
  }
  /* object is not a table, or it has a `settable' method */
  /* prepare arguments and call the tag method */
  *(top+2) = *(top-1);
  *(top+1) = *(t+1);
  *(top) = *t;
  *(top-1) = *im;
  L->top = top+3;
  luaD_call(L, top-1, 0);
}


void luaV_rawsettable (lua_State *L, StkId t) {
  if (ttype(t) != LUA_T_ARRAY)
    lua_error(L, "indexed expression not a table");
  else {
    luaH_set(L, avalue(t), t+1, L->top-1);
    L->top -= 3;
  }
}


/*
** WARNING: caller must assure 3 extra stack slots (to call a tag method)
*/
void luaV_getglobal (lua_State *L, GlobalVar *gv, StkId top) {
  const TObject *value = &gv->value;
  TObject *im = luaT_getimbyObj(L, value, IM_GETGLOBAL);
  if (ttype(im) == LUA_T_NIL)  /* is there a tag method? */
    *top = *value;  /* default behavior */
  else {  /* tag method */
    *top = *im;
    ttype(top+1) = LUA_T_STRING;
    tsvalue(top+1) = gv->name;  /* global name */
    *(top+2) = *value;
    L->top = top+3;
    luaD_call(L, top, 1);
  }
}


/*
** WARNING: caller must assure 3 extra stack slots (to call a tag method)
*/
void luaV_setglobal (lua_State *L, GlobalVar *gv, StkId top) {
  const TObject *oldvalue = &gv->value;
  const TObject *im = luaT_getimbyObj(L, oldvalue, IM_SETGLOBAL);
  if (ttype(im) == LUA_T_NIL)  /* is there a tag method? */
    gv->value = *(top-1);
  else {
    *(top+2) = *(top-1);  /* new value */
    *(top+1) = *oldvalue;
    ttype(top) = LUA_T_STRING;
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
  if (ttype(im) == LUA_T_NIL) {
    im = luaT_getimbyObj(L, top-1, event);  /* try second operand */
    if (ttype(im) == LUA_T_NIL) {
      im = luaT_getim(L, 0, event);  /* try a `global' method */
      if (ttype(im) == LUA_T_NIL)
        lua_error(L, msg);
    }
  }
  lua_pushstring(L, luaT_eventname[event]);
  luaD_callTM(L, im, 3, 1);
}


static void call_arith (lua_State *L, StkId top, IMS event) {
  call_binTM(L, top, event, "unexpected type in arithmetic operation");
}


static int luaV_strcomp (const TaggedString *ls, const TaggedString *rs) {
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

void luaV_comparison (lua_State *L) {
  const TObject *l = L->top-2;
  const TObject *r = L->top-1;
  int result;
  if (ttype(l) == LUA_T_NUMBER && ttype(r) == LUA_T_NUMBER)
    result = nvalue(l) < nvalue(r);
  else if (ttype(l) == LUA_T_STRING && ttype(r) == LUA_T_STRING)
    result = luaV_strcomp(tsvalue(l), tsvalue(r)) < 0;
  else {
    call_binTM(L, L->top, IM_LT, "unexpected type in comparison");
    return;
  }
  L->top--;
  if (result) {
    nvalue(L->top-1) = 1.0;
    ttype(L->top-1) = LUA_T_NUMBER;
  }
  else
    ttype(L->top-1) = LUA_T_NIL;
}


#define setbool(o,cond) if (cond) { \
                              ttype(o) = LUA_T_NUMBER; nvalue(o) = 1.0; } \
                        else ttype(o) = LUA_T_NIL


void luaV_pack (lua_State *L, StkId firstelem, int nvararg, TObject *tab) {
  int i;
  Hash *htab;
  htab = avalue(tab) = luaH_new(L, nvararg+1);  /* +1 for field `n' */
  ttype(tab) = LUA_T_ARRAY;
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
StkId luaV_execute (lua_State *L, const Closure *cl, const TProtoFunc *tf,
                    register StkId base) {
  register StkId top;  /* keep top local, for performance */
  register const Instruction *pc = tf->code;
  TaggedString **kstr = tf->kstr;
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

      case ENDCODE:
        return L->top;  /* no results */

      case RETCODE:
        L->top = top;
        return base+GETARG_U(i);

      case CALL:
        L->top = top;
        luaD_call(L, base+GETARG_A(i), GETARG_B(i));
        top = L->top;
        break;

      case TAILCALL:
        L->top = top;
        luaD_call(L, base+GETARG_A(i), MULT_RET);
        return base+GETARG_B(i);

      case PUSHNIL: {
        register int n = GETARG_U(i);
        do {
          ttype(top++) = LUA_T_NIL;
        } while (n--);
        break;
      }

      case POP:
        top -= GETARG_U(i);
        break;

      case PUSHINT:
        ttype(top) = LUA_T_NUMBER;
        nvalue(top) = (real)GETARG_S(i);
        top++;
        break;

      case PUSHSTRING:
        ttype(top) = LUA_T_STRING;
        tsvalue(top) = kstr[GETARG_U(i)];
        top++;
        break;

      case PUSHNUMBER:
        ttype(top) = LUA_T_NUMBER;
        nvalue(top) = tf->knum[GETARG_U(i)];
        top++;
        break;

      case PUSHUPVALUE:
        *top++ = cl->consts[GETARG_U(i)+1];
        break;

      case PUSHLOCAL:
        *top++ = *(base+GETARG_U(i));
        break;

      case GETGLOBAL:
        luaV_getglobal(L, kstr[GETARG_U(i)]->u.s.gv, top);
        top++;
        break;

      case GETTABLE:
        luaV_gettable(L, top);
        top--;
        break;

      case GETDOTTED:
        ttype(top) = LUA_T_STRING;
        tsvalue(top++) = kstr[GETARG_U(i)];
        luaV_gettable(L, top);
        top--;
        break;

      case PUSHSELF: {
        TObject receiver;
        receiver = *(top-1);
        ttype(top) = LUA_T_STRING;
        tsvalue(top++) = kstr[GETARG_U(i)];
        luaV_gettable(L, top);
        *(top-1) = receiver;
        break;
      }

      case CREATETABLE:
        L->top = top;
        luaC_checkGC(L);
        avalue(top) = luaH_new(L, GETARG_U(i));
        ttype(top) = LUA_T_ARRAY;
        top++;
        break;

      case SETLOCAL:
        *(base+GETARG_U(i)) = *(--top);
        break;

      case SETGLOBAL:
        luaV_setglobal(L, kstr[GETARG_U(i)]->u.s.gv, top);
        top--;
        break;

      case SETTABLEPOP:
        luaV_settable(L, top-3, top);
        top -= 3;  /* pop table, index, and value */
        break;

      case SETTABLE:
        luaV_settable(L, top-3-GETARG_U(i), top);
        top--;  /* pop value */
        break;

      case SETLIST: {
        int aux = GETARG_A(i) * LFIELDS_PER_FLUSH;
        int n = GETARG_B(i)+1;
        Hash *arr = avalue(top-n-1);
        L->top = top-n;  /* final value of `top' (in case of errors) */
        for (; n; n--)
          luaH_setint(L, arr, n+aux, --top);
        break;
      }

      case SETMAP: {
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

      case NEQOP:
        top--;
        setbool(top-1, !luaO_equalObj(top-1, top));
        break;

      case EQOP:
        top--;
        setbool(top-1, luaO_equalObj(top-1, top));
        break;

      case LTOP:
        top--;
        if (ttype(top-1) == LUA_T_NUMBER && ttype(top) == LUA_T_NUMBER)
          setbool(top-1, nvalue(top-1) < nvalue(top));
        else if (ttype(top-1) == LUA_T_STRING && ttype(top) == LUA_T_STRING)
          setbool(top-1, luaV_strcomp(tsvalue(top-1), tsvalue(top)) < 0);
        else
          call_binTM(L, top+1, IM_LT, "unexpected type in comparison");
        break;

      case LEOP:
        top--;
        if (ttype(top-1) == LUA_T_NUMBER && ttype(top) == LUA_T_NUMBER)
          setbool(top-1, nvalue(top-1) <= nvalue(top));
        else if (ttype(top-1) == LUA_T_STRING && ttype(top) == LUA_T_STRING)
          setbool(top-1, luaV_strcomp(tsvalue(top-1), tsvalue(top)) <= 0);
        else
          call_binTM(L, top+1, IM_LE, "unexpected type in comparison");
        break;

      case GTOP:
        top--;
        if (ttype(top-1) == LUA_T_NUMBER && ttype(top) == LUA_T_NUMBER)
          setbool(top-1, nvalue(top-1) > nvalue(top));
        else if (ttype(top-1) == LUA_T_STRING && ttype(top) == LUA_T_STRING)
          setbool(top-1, luaV_strcomp(tsvalue(top-1), tsvalue(top)) > 0);
        else
          call_binTM(L, top+1, IM_GT, "unexpected type in comparison");
        break;

      case GEOP:
        top--;
        if (ttype(top-1) == LUA_T_NUMBER && ttype(top) == LUA_T_NUMBER)
          setbool(top-1, nvalue(top-1) >= nvalue(top));
        else if (ttype(top-1) == LUA_T_STRING && ttype(top) == LUA_T_STRING)
          setbool(top-1, luaV_strcomp(tsvalue(top-1), tsvalue(top)) >= 0);
        else
          call_binTM(L, top+1, IM_GE, "unexpected type in comparison");
        break;

      case ADDOP:
        if (tonumber(top-1) || tonumber(top-2))
          call_arith(L, top, IM_ADD);
        else
          nvalue(top-2) += nvalue(top-1);
        top--;
        break;

      case SUBOP:
        if (tonumber(top-1) || tonumber(top-2))
          call_arith(L, top, IM_SUB);
        else
          nvalue(top-2) -= nvalue(top-1);
        top--;
        break;

      case MULTOP:
        if (tonumber(top-1) || tonumber(top-2))
          call_arith(L, top, IM_MUL);
        else
          nvalue(top-2) *= nvalue(top-1);
        top--;
        break;

      case DIVOP:
        if (tonumber(top-1) || tonumber(top-2))
          call_arith(L, top, IM_DIV);
        else
          nvalue(top-2) /= nvalue(top-1);
        top--;
        break;

      case POWOP:
        call_binTM(L, top, IM_POW, "undefined operation");
        top--;
        break;

      case CONCOP:
        if (tostring(L, top-2) || tostring(L, top-1))
          call_binTM(L, top, IM_CONCAT, "unexpected type for concatenation");
        else
          tsvalue(top-2) = strconc(L, tsvalue(top-2), tsvalue(top-1));
        top--;
        L->top = top;
        luaC_checkGC(L);
        break;

      case MINUSOP:
        if (tonumber(top-1)) {
          ttype(top) = LUA_T_NIL;
          call_arith(L, top+1, IM_UNM);
        }
        else
          nvalue(top-1) = - nvalue(top-1);
        break;

      case NOTOP:
        ttype(top-1) =
           (ttype(top-1) == LUA_T_NIL) ? LUA_T_NUMBER : LUA_T_NIL;
        nvalue(top-1) = 1;
        break;

      case ONTJMP:
        if (ttype(top-1) != LUA_T_NIL) pc += GETARG_S(i);
        else top--;
        break;

      case ONFJMP:
        if (ttype(top-1) == LUA_T_NIL) pc += GETARG_S(i);
        else top--;
        break;

      case JMP:
        pc += GETARG_S(i);
        break;

      case IFTJMP:
        if (ttype(--top) != LUA_T_NIL) pc += GETARG_S(i);
        break;

      case IFFJMP:
        if (ttype(--top) == LUA_T_NIL) pc += GETARG_S(i);
        break;

      case CLOSURE:
        ttype(top) = LUA_T_LPROTO;
        tfvalue(top) = tf->kproto[GETARG_A(i)];
        L->top = ++top;
        luaV_closure(L, GETARG_B(i));
        top -= GETARG_B(i);
        luaC_checkGC(L);
        break;

      case SETLINE:
        if ((base-1)->ttype != LUA_T_LINE) {
          /* open space for LINE value */
          int n = top-base;
          while (n--) base[n+1] = base[n];
          base++;
          top++;
          (base-1)->ttype = LUA_T_LINE;
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
