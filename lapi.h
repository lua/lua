/*
** $Id: lapi.h,v 1.21 2002/03/04 21:29:41 roberto Exp roberto $
** Auxiliary functions from Lua API
** See Copyright Notice in lua.h
*/

#ifndef lapi_h
#define lapi_h


#include "lobject.h"


void luaA_pushobject (lua_State *L, const TValue *o);

#endif
