/*
** $Id: ltm.c,v 1.88 2002/03/18 20:24:14 roberto Exp roberto $
** Tag methods
** See Copyright Notice in lua.h
*/


#include <stdio.h>
#include <string.h>

#include "lua.h"

#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"



const char *const luaT_typenames[] = {
  "nil", "number", "string", "boolean", "table",
  "userdata", "userdata", "function"
};


void luaT_init (lua_State *L) {
  static const char *const luaT_eventname[] = {  /* ORDER TM */
    "__gettable", "__settable", "__index",
    "__gc", "__weakmode",
    "__add", "__sub", "__mul", "__div",
    "__pow", "__unm", "__lt", "__concat",
    "__call"
  };
  int i;
  for (i=0; i<TM_N; i++) {
    G(L)->tmname[i] = luaS_new(L, luaT_eventname[i]);
    G(L)->tmname[i]->tsv.marked = FIXMARK;  /* never collect these names */
  }
}


/*
** function to be used with macro "fasttm": optimized for absence of
** tag methods
*/
const TObject *luaT_gettm (Table *events, TMS event, TString *ename) {
  const TObject *tm = luaH_getstr(events, ename);
  if (ttype(tm) == LUA_TNIL) {  /* no tag method? */
    events->flags |= (1<<event);  /* cache this fact */
    return NULL;
  }
  else return tm;
}


const TObject *luaT_gettmbyobj (lua_State *L, const TObject *o, TMS event) {
  TString *ename = G(L)->tmname[event];
  switch (ttype(o)) {
    case LUA_TTABLE:
      return luaH_getstr(hvalue(o)->metatable, ename);
    case LUA_TUSERDATA:
      return luaH_getstr(uvalue(o)->uv.metatable, ename);
    default:
      return &luaO_nilobject;
  }
}

