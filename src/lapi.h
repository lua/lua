/*
** $Id: lapi.h,v 2.1 2003/12/10 12:13:36 roberto Exp $
** Auxiliary functions from Lua API
** See Copyright Notice in lua.h
*/

#ifndef lapi_h
#define lapi_h


#include "lobject.h"


void luaA_pushobject (lua_State *L, const TValue *o);

#endif
