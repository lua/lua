/*
** $Id: lapi.h,v 1.17 2000/05/08 19:32:53 roberto Exp roberto $
** Auxiliary functions from Lua API
** See Copyright Notice in lua.h
*/

#ifndef lapi_h
#define lapi_h


#include "lobject.h"


void luaA_checkCargs (lua_State *L, int nargs);
void luaA_pushobject (lua_State *L, const TObject *o);
int luaA_next (lua_State *L, const Hash *t, int i);
lua_Object luaA_putluaObject (lua_State *L, const TObject *o);

#endif
