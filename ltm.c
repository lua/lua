/*
** $Id: ltm.c,v 2.17 2013/04/25 16:07:52 roberto Exp roberto $
** Tag methods
** See Copyright Notice in lua.h
*/


#include <string.h>

#define ltm_c
#define LUA_CORE

#include "lua.h"

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
  "proto", "upval"  /* these last two cases are used for tests only */
};


void luaT_init (lua_State *L) {
  static const char *const luaT_eventname[] = {  /* ORDER TM */
    "__index", "__newindex",
    "__gc", "__mode", "__len", "__eq",
    "__add", "__sub", "__mul", "__div", "__idiv", "__mod",
    "__pow", "__unm", "__lt", "__le",
    "__concat", "__call"
  };
  int i;
  for (i=0; i<TM_N; i++) {
    G(L)->tmname[i] = luaS_new(L, luaT_eventname[i]);
    luaS_fix(G(L)->tmname[i]);  /* never collect these names */
  }
}


/*
** function to be used with macro "fasttm": optimized for absence of
** tag methods
*/
const TValue *luaT_gettm (Table *events, TMS event, TString *ename) {
  const TValue *tm = luaH_getstr(events, ename);
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
  return (mt ? luaH_getstr(mt, G(L)->tmname[event]) : luaO_nilobject);
}


void luaT_callTM (lua_State *L, const TValue *f, const TValue *p1,
                  const TValue *p2, TValue *p3, int hasres) {
  ptrdiff_t result = savestack(L, p3);
  setobj2s(L, L->top++, f);  /* push function */
  setobj2s(L, L->top++, p1);  /* 1st argument */
  setobj2s(L, L->top++, p2);  /* 2nd argument */
  if (!hasres)  /* no result? 'p3' is third argument */
    setobj2s(L, L->top++, p3);  /* 3rd argument */
  /* metamethod may yield only when called from Lua code */
  luaD_call(L, L->top - (4 - hasres), hasres, isLua(L->ci));
  if (hasres) {  /* if has result, move it to its place */
    p3 = restorestack(L, result);
    setobjs2s(L, p3, --L->top);
  }
}


int luaT_callbinTM (lua_State *L, const TValue *p1, const TValue *p2,
                    StkId res, TMS event) {
  const TValue *tm = luaT_gettmbyobj(L, p1, event);  /* try first operand */
  if (ttisnil(tm))
    tm = luaT_gettmbyobj(L, p2, event);  /* try second operand */
  if (ttisnil(tm)) return 0;
  luaT_callTM(L, tm, p1, p2, res, 1);
  return 1;
}


const TValue *luaT_getequalTM (lua_State *L, Table *mt1, Table *mt2) {
  const TValue *tm1 = fasttm(L, mt1, TM_EQ);
  const TValue *tm2;
  if (tm1 == NULL) return NULL;  /* no metamethod */
  if (mt1 == mt2) return tm1;  /* same metatables => same metamethods */
  tm2 = fasttm(L, mt2, TM_EQ);
  if (tm2 == NULL) return NULL;  /* no metamethod */
  if (luaV_rawequalobj(tm1, tm2))  /* same metamethods? */
    return tm1;
  return NULL;
}


int luaT_callorderTM (lua_State *L, const TValue *p1, const TValue *p2,
                      TMS event) {
  if (!luaT_callbinTM(L, p1, p2, L->top, event))
    return -1;  /* no metamethod */
  else
    return !l_isfalse(L->top);
}

