/*
** $Id: lapi.h,v 1.9 1999/11/22 13:12:07 roberto Exp roberto $
** Auxiliary functions from Lua API
** See Copyright Notice in lua.h
*/

#ifndef lapi_h
#define lapi_h


#include "lobject.h"


void luaA_pushobject (lua_State *L, const TObject *o);
GlobalVar *luaA_nextvar (lua_State *L, TaggedString *g);
int luaA_next (lua_State *L, const Hash *t, int i);
lua_Object luaA_putObjectOnTop (lua_State *L);

#endif
