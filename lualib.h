/*
** $Id: lualib.h,v 1.30 2004/05/28 18:35:05 roberto Exp roberto $
** Lua standard libraries
** See Copyright Notice in lua.h
*/


#ifndef lualib_h
#define lualib_h

#include "lua.h"


/* Key to file-handle type */
#define LUA_FILEHANDLE		"FILE*"


#define LUA_COLIBNAME	"coroutine"
LUALIB_API int luaopen_base (lua_State *L);

#define LUA_TABLIBNAME	"table"
LUALIB_API int luaopen_table (lua_State *L);

#define LUA_IOLIBNAME	"io"
#define LUA_OSLIBNAME	"os"
LUALIB_API int luaopen_io (lua_State *L);

#define LUA_STRLIBNAME	"string"
LUALIB_API int luaopen_string (lua_State *L);

#define LUA_MATHLIBNAME	"math"
LUALIB_API int luaopen_math (lua_State *L);

#define LUA_DBLIBNAME	"debug"
LUALIB_API int luaopen_debug (lua_State *L);


LUALIB_API int luaopen_loadlib (lua_State *L);


/* open all previous libraries */
LUALIB_API int luaopen_stdlibs (lua_State *L); 


#endif
