/*
** $Id: ltm.h,v 1.8 1999/11/22 13:12:07 roberto Exp roberto $
** Tag methods
** See Copyright Notice in lua.h
*/

#ifndef ltm_h
#define ltm_h


#include "lobject.h"
#include "lstate.h"

/*
* WARNING: if you change the order of this enumeration,
* grep "ORDER IM"
*/
typedef enum {
  IM_GETTABLE = 0,
  IM_SETTABLE,
  IM_INDEX,
  IM_GETGLOBAL,
  IM_SETGLOBAL,
  IM_ADD,
  IM_SUB,
  IM_MUL,
  IM_DIV,
  IM_POW,
  IM_UNM,
  IM_LT,
  IM_CONCAT,
  IM_GC,
  IM_FUNCTION
} IMS;

#define IM_N 15


struct IM {
  TObject int_method[IM_N];
};


#define luaT_getim(L,tag,event) (&L->IMtable[-(tag)].int_method[event])
#define luaT_getimbyObj(L,o,e)  (luaT_getim(L, luaT_effectivetag(o),(e)))

extern const char *const luaT_eventname[];


void luaT_init (lua_State *L);
void luaT_realtag (lua_State *L, int tag);
int luaT_effectivetag (const TObject *o);
void luaT_settagmethod (lua_State *L, int t, const char *event, TObject *func);
const TObject *luaT_gettagmethod (lua_State *L, int t, const char *event);
const char *luaT_travtagmethods (lua_State *L, int (*fn)(lua_State *, TObject *));

int luaT_validevent (int t, int e);

#endif
