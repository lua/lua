/*
** $Id: ltm.c,v 2.55 2017/12/20 14:58:05 roberto Exp roberto $
** Tag methods
** See Copyright Notice in lua.h
*/

#define ltm_c
#define LUA_CORE

#include "lprefix.h"


#include <string.h>

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lvm.h"


static const char udatatypename[] = "userdata";

LUAI_DDEF const char *const luaT_typenames_[LUA_TOTALTAGS] = {
  "no value",
  "nil", "boolean", udatatypename, "number",
  "string", "table", "function", udatatypename, "thread",
  "upvalue", "proto" /* these last cases are used for tests only */
};


void luaT_init (lua_State *L) {
  static const char *const luaT_eventname[] = {  /* ORDER TM */
    "__index", "__newindex",
    "__gc", "__mode", "__len", "__eq",
    "__add", "__sub", "__mul", "__mod", "__pow",
    "__div", "__idiv",
    "__band", "__bor", "__bxor", "__shl", "__shr",
    "__unm", "__bnot", "__lt", "__le",
    "__concat", "__call"
  };
  int i;
  for (i=0; i<TM_N; i++) {
    G(L)->tmname[i] = luaS_new(L, luaT_eventname[i]);
    luaC_fix(L, obj2gco(G(L)->tmname[i]));  /* never collect these names */
  }
}


/*
** function to be used with macro "fasttm": optimized for absence of
** tag methods
*/
const TValue *luaT_gettm (Table *events, TMS event, TString *ename) {
  const TValue *tm = luaH_getshortstr(events, ename);
  lua_assert(event <= TM_EQ);
  if (ttisnil(tm)) {  /* no tag method? */
    events->flags |= cast_byte(1u<<event);  /* cache this fact */
    return NULL;
  }
  else return tm;
}


const TValue *luaT_gettmbyobj (lua_State *L, const TValue *o, TMS event) {
  Table *mt;
  switch (ttnov(o)) {
    case LUA_TTABLE:
      mt = hvalue(o)->metatable;
      break;
    case LUA_TUSERDATA:
      mt = uvalue(o)->metatable;
      break;
    default:
      mt = G(L)->mt[ttnov(o)];
  }
  return (mt ? luaH_getshortstr(mt, G(L)->tmname[event]) : luaO_nilobject);
}


/*
** Return the name of the type of an object. For tables and userdata
** with metatable, use their '__name' metafield, if present.
*/
const char *luaT_objtypename (lua_State *L, const TValue *o) {
  Table *mt;
  if ((ttistable(o) && (mt = hvalue(o)->metatable) != NULL) ||
      (ttisfulluserdata(o) && (mt = uvalue(o)->metatable) != NULL)) {
    const TValue *name = luaH_getshortstr(mt, luaS_new(L, "__name"));
    if (ttisstring(name))  /* is '__name' a string? */
      return getstr(tsvalue(name));  /* use it as type name */
  }
  return ttypename(ttnov(o));  /* else use standard type name */
}


void luaT_callTM (lua_State *L, const TValue *f, const TValue *p1,
                  const TValue *p2, const TValue *p3) {
  StkId func = L->top;
  setobj2s(L, func, f);  /* push function (assume EXTRA_STACK) */
  setobj2s(L, func + 1, p1);  /* 1st argument */
  setobj2s(L, func + 2, p2);  /* 2nd argument */
  setobj2s(L, func + 3, p3);  /* 3rd argument */
  L->top = func + 4;
  /* metamethod may yield only when called from Lua code */
  if (isLuacode(L->ci))
    luaD_call(L, func, 0);
  else
    luaD_callnoyield(L, func, 0);
}


void luaT_callTMres (lua_State *L, const TValue *f, const TValue *p1,
                     const TValue *p2, StkId res) {
  ptrdiff_t result = savestack(L, res);
  StkId func = L->top;
  setobj2s(L, func, f);  /* push function (assume EXTRA_STACK) */
  setobj2s(L, func + 1, p1);  /* 1st argument */
  setobj2s(L, func + 2, p2);  /* 2nd argument */
  L->top += 3;
  /* metamethod may yield only when called from Lua code */
  if (isLuacode(L->ci))
    luaD_call(L, func, 1);
  else
    luaD_callnoyield(L, func, 1);
  res = restorestack(L, result);
  setobjs2s(L, res, --L->top);  /* move result to its place */
}


static int callbinTM (lua_State *L, const TValue *p1, const TValue *p2,
                      StkId res, TMS event) {
  const TValue *tm = luaT_gettmbyobj(L, p1, event);  /* try first operand */
  if (ttisnil(tm))
    tm = luaT_gettmbyobj(L, p2, event);  /* try second operand */
  if (ttisnil(tm)) return 0;
  luaT_callTMres(L, tm, p1, p2, res);
  return 1;
}


void luaT_trybinTM (lua_State *L, const TValue *p1, const TValue *p2,
                    StkId res, TMS event) {
  if (!callbinTM(L, p1, p2, res, event)) {
    switch (event) {
      case TM_CONCAT:
        luaG_concaterror(L, p1, p2);
      /* call never returns, but to avoid warnings: *//* FALLTHROUGH */
      case TM_BAND: case TM_BOR: case TM_BXOR:
      case TM_SHL: case TM_SHR: case TM_BNOT: {
        if (ttisnumber(p1) && ttisnumber(p2))
          luaG_tointerror(L, p1, p2);
        else
          luaG_opinterror(L, p1, p2, "perform bitwise operation on");
      }
      /* calls never return, but to avoid warnings: *//* FALLTHROUGH */
      default:
        luaG_opinterror(L, p1, p2, "perform arithmetic on");
    }
  }
}


void luaT_trybinassocTM (lua_State *L, const TValue *p1, const TValue *p2,
                                       StkId res, int inv, TMS event) {
  if (inv)
    luaT_trybinTM(L, p2, p1, res, event);
  else
    luaT_trybinTM(L, p1, p2, res, event);
}


void luaT_trybiniTM (lua_State *L, const TValue *p1, int i2,
                                   int inv, StkId res, TMS event) {
  TValue aux;
  setivalue(&aux, i2);
  luaT_trybinassocTM(L, p1, &aux, res, inv, event);
}


int luaT_callorderTM (lua_State *L, const TValue *p1, const TValue *p2,
                      TMS event) {
  if (callbinTM(L, p1, p2, L->top, event))  /* try original event */
    return !l_isfalse(s2v(L->top));
  else if (event == TM_LE) {
      /* try '!(p2 < p1)' for '(p1 <= p2)' */
      L->ci->callstatus |= CIST_LEQ;  /* mark it is doing 'lt' for 'le' */
      if (callbinTM(L, p2, p1, L->top, TM_LT)) {
        L->ci->callstatus ^= CIST_LEQ;  /* clear mark */
        return l_isfalse(s2v(L->top));
      }
      /* else error will remove this 'ci'; no need to clear mark */
  }
  luaG_ordererror(L, p1, p2);  /* no metamethod found */
  return 0;  /* to avoid warnings */
}


int luaT_callorderiTM (lua_State *L, const TValue *p1, int v2,
                       int inv, TMS event) {
  TValue aux; const TValue *p2;
  setivalue(&aux, v2);
  if (inv) {  /* arguments were exchanged? */
    p2 = p1; p1 = &aux;  /* correct them */
    event = (event == TM_LE) ? TM_LT : TM_LE;
  }
  else
    p2 = &aux;
  return (luaT_callorderTM(L, p1, p2, event) != inv);
}


void luaT_adjustvarargs (lua_State *L, Proto *p, int actual) {
  int i;
  Table *vtab;
  TValue nname;
  int nfixparams = p->numparams;  /* number of fixed parameters */
  actual -= nfixparams;  /* number of extra arguments */
  vtab = luaH_new(L);  /* create vararg table */
  sethvalue2s(L, L->top, vtab);  /* anchor it for resizing */
  L->top++;  /* space ensured by caller */
  luaH_resize(L, vtab, actual, 1);
  for (i = 0; i < actual; i++)  /* put extra arguments into vararg table */
    setobj2n(L, &vtab->array[i], s2v(L->top - actual + i - 1));
  setsvalue(L, &nname, G(L)->nfield);  /* get field 'n' */
  setivalue(luaH_set(L, vtab, &nname), actual);  /* store counter there */
  L->top -= actual;  /* remove extra elements from the stack */
  sethvalue2s(L, L->top - 1, vtab);  /* move table to new top */
}


void luaT_getvarargs (lua_State *L, TValue *t, StkId where, int wanted) {
  if (!ttistable(t))
    luaG_runerror(L, "'vararg' parameter is not a table");
  else {
    int i;
    Table *h = hvalue(t);
    if (wanted < 0) {  /* get all? */
      const TValue *ns = luaH_getstr(h, G(L)->nfield);
      int n = (ttisinteger(ns)) ? cast_int(ivalue(ns)) : 0;
      wanted = n;
      checkstackp(L, n, where);
      L->top = where + n;
    }
    for (i = 0; i < wanted; i++)  /* get what is available */
      setobj2s(L, where + i, luaH_getint(h, i + 1));
    return;
  }
}
