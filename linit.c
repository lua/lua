/*
** $Id: linit.c,v 1.3 1999/12/06 11:43:09 roberto Exp roberto $
** Initialization of libraries for lua.c
** See Copyright Notice in lua.h
*/

#define LUA_REENTRANT

#include "lua.h"

#include "lualib.h"


void lua_userinit (lua_State *L) {
  lua_beginblock(L);
  lua_iolibopen(L);
  lua_strlibopen(L);
  lua_mathlibopen(L);
  lua_dblibopen(L);
  lua_endblock(L);
}

