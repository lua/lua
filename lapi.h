/*
** $Id: lapi.h,v 1.8 1999/11/04 17:22:26 roberto Exp roberto $
** Auxiliary functions from Lua API
** See Copyright Notice in lua.h
*/

#ifndef lapi_h
#define lapi_h


#include "lobject.h"


TObject *luaA_Address (lua_State *L, lua_Object o);
void luaA_pushobject (lua_State *L, const TObject *o);
GlobalVar *luaA_nextvar (lua_State *L, TaggedString *g);
int luaA_next (lua_State *L, const Hash *t, int i);
lua_Object luaA_putObjectOnTop (lua_State *L);

#endif
