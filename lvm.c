/*
** $Id: lvm.c,v 1.83 2000/01/25 13:57:18 roberto Exp roberto $
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
                    StkId base) {
  register StkId top;  /* keep top local, for performance */
  register const Byte *pc = tf->code;
  TaggedString **kstr = tf->kstr;
  if (L->callhook)
    luaD_callHook(L, base-1, L->callhook, "call");
  luaD_checkstack(L, (*pc++)+EXTRA_STACK);
  if (*pc < ZEROVARARG)
    luaD_adjusttop(L, base, *(pc++));
  else {  /* varargs */
    adjust_varargs(L, base, (*pc++)-ZEROVARARG);
    luaC_checkGC(L);
  }
  top = L->top;
  for (;;) {
    register int aux = 0;
    switchentry:
    switch ((OpCode)*pc++) {

      case ENDCODE:
        return L->top;  /* no results */

      case RETCODE:
        L->top = top;
        return base+(*pc++);

      case CALL: aux = *pc++;
        L->top = top;
        luaD_call(L, base+(*pc++), aux);
        top = L->top;
        break;

      case TAILCALL: aux = *pc++;
        L->top = top;
        luaD_call(L, base+(*pc++), MULT_RET);
        return base+aux;

      case PUSHNIL: aux = *pc++;
        do {
          ttype(top++) = LUA_T_NIL;
        } while (aux--);
        break;

      case POP: aux = *pc++;
        top -= aux;
        break;

      case PUSHINTW: aux += highbyte(L, *pc++);
      case PUSHINT:  aux += *pc++;
        ttype(top) = LUA_T_NUMBER;
        nvalue(top) = aux;
        top++;
        break;

      case PUSHINTNEGW: aux += highbyte(L, *pc++);
      case PUSHINTNEG:  aux += *pc++;
        ttype(top) = LUA_T_NUMBER;
        nvalue(top) = -aux;
        top++;
        break;

      case PUSHSTRINGW: aux += highbyte(L, *pc++);
      case PUSHSTRING:  aux += *pc++;
        ttype(top) = LUA_T_STRING;
        tsvalue(top) = kstr[aux];
        top++;
        break;

      case PUSHNUMBERW: aux += highbyte(L, *pc++);
      case PUSHNUMBER:  aux += *pc++;
        ttype(top) = LUA_T_NUMBER;
        nvalue(top) = tf->knum[aux];
        top++;
        break;

      case PUSHUPVALUE: aux = *pc++;
        *top++ = cl->consts[aux+1];
        break;

      case PUSHLOCAL: aux = *pc++;
        *top++ = *(base+aux);
        break;

      case GETGLOBALW: aux += highbyte(L, *pc++);
      case GETGLOBAL:  aux += *pc++;
        luaV_getglobal(L, kstr[aux]->u.s.gv, top);
        top++;
        break;

      case GETTABLE:
        luaV_gettable(L, top);
        top--;
        break;

      case GETDOTTEDW: aux += highbyte(L, *pc++);
      case GETDOTTED:  aux += *pc++;
        ttype(top) = LUA_T_STRING;
        tsvalue(top++) = kstr[aux];
        luaV_gettable(L, top);
        top--;
        break;

      case PUSHSELFW: aux += highbyte(L, *pc++);
      case PUSHSELF:  aux += *pc++; {
        TObject receiver;
        receiver = *(top-1);
        ttype(top) = LUA_T_STRING;
        tsvalue(top++) = kstr[aux];
        luaV_gettable(L, top);
        *(top-1) = receiver;
        break;
      }

      case CREATEARRAYW: aux += highbyte(L, *pc++);
      case CREATEARRAY:  aux += *pc++;
        L->top = top;
        luaC_checkGC(L);
        avalue(top) = luaH_new(L, aux);
        ttype(top) = LUA_T_ARRAY;
        top++;
        break;

      case SETLOCAL: aux = *pc++;
        *(base+aux) = *(--top);
        break;

      case SETGLOBALW: aux += highbyte(L, *pc++);
      case SETGLOBAL:  aux += *pc++;
        luaV_setglobal(L, kstr[aux]->u.s.gv, top);
        top--;
        break;

      case SETTABLEPOP:
        luaV_settable(L, top-3, top);
        top -= 3;  /* pop table, index, and value */
        break;

      case SETTABLE:
        luaV_settable(L, top-3-(*pc++), top);
        top--;  /* pop value */
        break;

      case SETLISTW: aux += highbyte(L, *pc++);
      case SETLIST:  aux += *pc++; {
        int n = *(pc++);
        Hash *arr = avalue(top-n-1);
        L->top = top-n;  /* final value of `top' (in case of errors) */
        aux *= LFIELDS_PER_FLUSH;
        for (; n; n--)
          luaH_setint(L, arr, n+aux, --top);
        break;
      }

      case SETMAP:  aux = *pc++; {
        StkId finaltop = top-2*(aux+1);
        Hash *arr = avalue(finaltop-1);
        L->top = finaltop;  /* final value of `top' (in case of errors) */
        do {
          luaH_set(L, arr, top-2, top-1);
          top-=2;
        } while (aux--);
        break;
      }

      case NEQOP: aux = 1;
      case EQOP:
        top--;
        aux = (luaO_equalObj(top-1, top) != aux);
      booleanresult:
        if (aux) {
          ttype(top-1) = LUA_T_NUMBER;
          nvalue(top-1) = 1.0;
        }
        else ttype(top-1) = LUA_T_NIL;
        break;

      case LTOP:
        top--;
        if (ttype(top-1) == LUA_T_NUMBER && ttype(top) == LUA_T_NUMBER)
          aux = nvalue(top-1) < nvalue(top);
        else if (ttype(top-1) == LUA_T_STRING && ttype(top) == LUA_T_STRING)
          aux = luaV_strcomp(tsvalue(top-1), tsvalue(top)) < 0;
        else {
          call_binTM(L, top+1, IM_LT, "unexpected type in comparison");
          break;
        }
        goto booleanresult;

      case LEOP:
        top--;
        if (ttype(top-1) == LUA_T_NUMBER && ttype(top) == LUA_T_NUMBER)
          aux = nvalue(top-1) <= nvalue(top);
        else if (ttype(top-1) == LUA_T_STRING && ttype(top) == LUA_T_STRING)
          aux = luaV_strcomp(tsvalue(top-1), tsvalue(top)) <= 0;
        else {
          call_binTM(L, top+1, IM_LE, "unexpected type in comparison");
          break;
        }
        goto booleanresult;

      case GTOP:
        top--;
        if (ttype(top-1) == LUA_T_NUMBER && ttype(top) == LUA_T_NUMBER)
          aux = nvalue(top-1) > nvalue(top);
        else if (ttype(top-1) == LUA_T_STRING && ttype(top) == LUA_T_STRING)
          aux = luaV_strcomp(tsvalue(top-1), tsvalue(top)) > 0;
        else {
          call_binTM(L, top+1, IM_GT, "unexpected type in comparison");
          break;
        }
        goto booleanresult;

      case GEOP:
        top--;
        if (ttype(top-1) == LUA_T_NUMBER && ttype(top) == LUA_T_NUMBER)
          aux = nvalue(top-1) >= nvalue(top);
        else if (ttype(top-1) == LUA_T_STRING && ttype(top) == LUA_T_STRING)
          aux = luaV_strcomp(tsvalue(top-1), tsvalue(top)) >= 0;
        else {
          call_binTM(L, top+1, IM_GE, "unexpected type in comparison");
          break;
        }
        goto booleanresult;

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

      case ONTJMPW: aux += highbyte(L, *pc++);
      case ONTJMP:  aux += *pc++;
        if (ttype(top-1) != LUA_T_NIL) pc += aux;
        else top--;
        break;

      case ONFJMPW: aux += highbyte(L, *pc++);
      case ONFJMP:  aux += *pc++;
        if (ttype(top-1) == LUA_T_NIL) pc += aux;
        else top--;
        break;

      case JMPW: aux += highbyte(L, *pc++);
      case JMP:  aux += *pc++;
        pc += aux;
        break;

      case IFFJMPW: aux += highbyte(L, *pc++);
      case IFFJMP:  aux += *pc++;
        if (ttype(--top) == LUA_T_NIL) pc += aux;
        break;

      case IFTUPJMPW: aux += highbyte(L, *pc++);
      case IFTUPJMP:  aux += *pc++;
        if (ttype(--top) != LUA_T_NIL) pc -= aux;
        break;

      case IFFUPJMPW: aux += highbyte(L, *pc++);
      case IFFUPJMP:  aux += *pc++;
        if (ttype(--top) == LUA_T_NIL) pc -= aux;
        break;

      case CLOSUREW: aux += highbyte(L, *pc++);
      case CLOSURE:  aux += *pc++;
        ttype(top) = LUA_T_LPROTO;
        tfvalue(top) = tf->kproto[aux];
        L->top = ++top;
        aux = *pc++;  /* number of upvalues */
        luaV_closure(L, aux);
        luaC_checkGC(L);
        top -= aux;
        break;

      case SETLINEW: aux += highbyte(L, *pc++);
      case SETLINE:  aux += *pc++;
        if ((base-2)->ttype != LUA_T_LINE) {
          /* open space for LINE and NAME values */
          int i = top-base;
          while (i--) base[i+2] = base[i];
          base += 2;
          top += 2;
          (base-1)->ttype = LUA_T_NIL;  /* initial value for NAME */
          (base-2)->ttype = LUA_T_LINE;
        }
        (base-2)->value.i = aux;
        if (L->linehook) {
          L->top = top;
          luaD_lineHook(L, base-3, aux);
        }
        break;

      case SETNAMEW: aux += highbyte(L, *pc++);
      case SETNAME:  aux += *pc++;
        if ((base-2)->ttype == LUA_T_LINE) {  /* function has debug info? */
          (base-1)->ttype = -(*pc++);
          (base-1)->value.i = aux;
        }
        break;

      case LONGARGW: aux += highbyte(L, *pc++);
      case LONGARG:  aux += *pc++;
        aux = highbyte(L, highbyte(L, aux));
        goto switchentry;  /* do not reset `aux' */

    }
  }
}
