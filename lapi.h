/*
** $Id: lapi.h,v 1.18 2000/05/08 20:49:05 roberto Exp roberto $
** Auxiliary functions from Lua API
** See Copyright Notice in lua.h
*/

#ifndef lapi_h
#define lapi_h


#include "lobject.h"


TObject *luaA_index (lua_State *L, int index);
void luaA_pushobject (lua_State *L, const TObject *o);
int luaA_next (lua_State *L, const Hash *t, int i);

#endif
