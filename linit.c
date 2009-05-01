/*
** $Id: linit.c,v 1.16 2008/01/02 16:16:28 roberto Exp roberto $
** Initialization of libraries for lua.c
** See Copyright Notice in lua.h
*/


#define linit_c
#define LUA_LIB

#include "lua.h"

#include "lualib.h"
#include "lauxlib.h"


/*
** these libs are loaded by lua.c and are readily available to any program
*/
static const luaL_Reg lualibs[] = {
  {"_G", luaopen_base},
  {LUA_LOADLIBNAME, luaopen_package},
  {LUA_TABLIBNAME, luaopen_table},
  {LUA_IOLIBNAME, luaopen_io},
  {LUA_OSLIBNAME, luaopen_os},
  {LUA_STRLIBNAME, luaopen_string},
  {LUA_MATHLIBNAME, luaopen_math},
  {NULL, NULL}
};


/*
** these libs are preloaded and must be required before used
*/
static const luaL_Reg luareqlibs[] = {
  {LUA_DBLIBNAME, luaopen_debug},
  {NULL, NULL}
};


LUALIB_API void luaL_openlibs (lua_State *L) {
  /* call open functions from 'lualibs' */
  const luaL_Reg *lib = lualibs;
  for (; lib->func; lib++) {
    lua_pushcfunction(L, lib->func);
    lua_pushstring(L, lib->name);
    lua_call(L, 1, 0);
  }
  /* add open functions from 'luareqlibs' into 'package.preload' table */
  lib = luareqlibs;
  luaL_findtable(L, LUA_GLOBALSINDEX, "package.preload", 0);
  for (; lib->func; lib++) {
    lua_pushcfunction(L, lib->func);
    lua_setfield(L, -2, lib->name);
  }
  lua_pop(L, 1);  /* remove package.preload table */
#ifdef LUA_COMPAT_DEBUGLIB
  lua_getglobal(L, "require");
  lua_pushliteral(L, LUA_DBLIBNAME);
  lua_call(L, 1, 0);  /* call 'require"debug"' */
#endif
}

