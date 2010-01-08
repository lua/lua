/*
** $Id: linit.c,v 1.23 2009/12/22 15:32:50 roberto Exp $
** Initialization of libraries for lua.c and other clients        
** See Copyright Notice in lua.h
*/


/*                                                           
** If you embed Lua in your program and need to open the standard
** libraries, call luaL_openlibs in your program. If you need a
** different set of libraries, copy this file to your project and edit
** it to suit your needs.
*/                                                              


#define linit_c
#define LUA_LIB

#include "lua.h"

#include "lualib.h"
#include "lauxlib.h"


/*
** these libs are loaded by lua.c and are readily available to any Lua
** program
*/
static const luaL_Reg loadedlibs[] = {
  {"_G", luaopen_base},
  {LUA_LOADLIBNAME, luaopen_package},
  {LUA_TABLIBNAME, luaopen_table},
  {LUA_IOLIBNAME, luaopen_io},
  {LUA_OSLIBNAME, luaopen_os},
  {LUA_STRLIBNAME, luaopen_string},
  {LUA_BITLIBNAME, luaopen_bit},
  {LUA_MATHLIBNAME, luaopen_math},
  {NULL, NULL}
};


/*
** these libs are preloaded and must be required before used
*/
static const luaL_Reg preloadedlibs[] = {
  {LUA_DBLIBNAME, luaopen_debug},
  {NULL, NULL}
};


LUALIB_API void luaL_openlibs (lua_State *L) {
  const luaL_Reg *lib;
  /* call open functions from 'loadedlibs' */
  for (lib = loadedlibs; lib->func; lib++) {
    lua_pushcfunction(L, lib->func);
    lua_pushstring(L, lib->name);
    lua_call(L, 1, 0);
  }
  /* add open functions from 'preloadedlibs' into 'package.preload' table */
  lua_pushglobaltable(L);
  luaL_findtable(L, 0, "package.preload", 0);
  for (lib = preloadedlibs; lib->func; lib++) {
    lua_pushcfunction(L, lib->func);
    lua_setfield(L, -2, lib->name);
  }
  lua_pop(L, 1);  /* remove package.preload table */
#if defined(LUA_COMPAT_DEBUGLIB)
  lua_pushglobaltable(L);
  lua_getfield(L, -1, "require");
  lua_pushliteral(L, LUA_DBLIBNAME);
  lua_call(L, 1, 0);  /* call 'require"debug"' */
  lua_pop(L, 1);  /* remove global table */
#endif
}

