/*
** $Id: linit.c,v 1.1 1999/01/08 16:49:32 roberto Exp roberto $
** Initialization of libraries for lua.c
** See Copyright Notice in lua.h
*/

#define LUA_REENTRANT

#include "lua.h"
#include "lualib.h"


void lua_userinit (lua_State *L) {
  lua_iolibopen(L);
  lua_strlibopen(L);
  lua_mathlibopen(L);
  lua_dblibopen(L);
}

