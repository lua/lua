/*
** $Id: lapi.h,v 1.15 2000/03/10 18:37:44 roberto Exp roberto $
** Auxiliary functions from Lua API
** See Copyright Notice in lua.h
*/

#ifndef lapi_h
#define lapi_h


#include "lobject.h"


void luaA_checkCargs (lua_State *L, int nargs);
void luaA_pushobject (lua_State *L, const TObject *o);
GlobalVar *luaA_nextvar (lua_State *L, TString *g);
int luaA_next (lua_State *L, const Hash *t, int i);
lua_Object luaA_putluaObject (lua_State *L, const TObject *o);
lua_Object luaA_putObjectOnTop (lua_State *L);

#endif
