/*
** $Id: linit.c,v 1.9 2005/02/18 12:40:02 roberto Exp roberto $
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
  int t = lua_gettop(L);
  lua_pushvalue(L, LUA_ENVIRONINDEX);  /* save original environment */
  for (; lib->func; lib++) {
    lib->func(L);  /* open library */
    lua_settop(L, t + 1);  /* discard any results */
    lua_pushvalue(L, -1);
    lua_replace(L, LUA_ENVIRONINDEX);  /* restore environment */
  }
  lua_pop(L, 1);
  return 0;
}

