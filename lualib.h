/*
** $Id: lualib.h,v 1.12 2000/09/12 13:46:59 roberto Exp roberto $
** Lua standard libraries
** See Copyright Notice in lua.h
*/


#ifndef lualib_h
#define lualib_h

#include "lua.h"


#define LUA_ALERT               "_ALERT"

LUA_API void lua_baselibopen (lua_State *L);
LUA_API void lua_iolibopen (lua_State *L);
LUA_API void lua_strlibopen (lua_State *L);
LUA_API void lua_mathlibopen (lua_State *L);
LUA_API void lua_dblibopen (lua_State *L);



/* Auxiliary functions (private) */

const char *luaI_classend (lua_State *L, const char *p);
int luaI_singlematch (int c, const char *p, const char *ep);

#endif
