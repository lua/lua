/*
** $Id: lualib.h,v 1.29 2004/03/24 15:46:49 roberto Exp roberto $
** Lua standard libraries
** See Copyright Notice in lua.h
*/


#ifndef lualib_h
#define lualib_h

#include "lua.h"


#ifndef LUALIB_API
#define LUALIB_API	LUA_API
#endif


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


/* to help testing the libraries */
#ifndef lua_assert
#define lua_assert(c)		/* empty */
#endif

#endif
