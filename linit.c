/*
** $Id: linit.c,v 1.7 2004/07/09 14:29:29 roberto Exp roberto $
** Initialization of libraries for lua.c
** See Copyright Notice in lua.h
*/


#define linit_c
#define LUA_LIB

#include "lua.h"

#include "lualib.h"
#include "lauxlib.h"


static const luaL_reg lualibs[] = {
  {"", luaopen_base},
  {LUA_TABLIBNAME, luaopen_table},
  {LUA_IOLIBNAME, luaopen_io},
  {LUA_OSLIBNAME, luaopen_os},
  {LUA_STRLIBNAME, luaopen_string},
  {LUA_MATHLIBNAME, luaopen_math},
  {LUA_DBLIBNAME, luaopen_debug},
  {"", luaopen_loadlib},
  {NULL, NULL}
};


LUALIB_API int luaopen_stdlibs (lua_State *L) {
  const luaL_reg *lib = lualibs;
  for (; lib->func; lib++) {
    lib->func(L);  /* open library */
    lua_settop(L, 0);  /* discard any results */
  }
 return 0;
}

