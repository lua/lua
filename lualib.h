/*
** $Id: lualib.h,v 1.15 2000/11/23 13:49:35 roberto Exp roberto $
** Lua standard libraries
** See Copyright Notice in lua.h
*/


#ifndef lualib_h
#define lualib_h

#include "lua.h"


#ifndef LUALIB_API
#define LUALIB_API	extern
#endif


#define LUA_ALERT               "_ALERT"

LUALIB_API void lua_baselibopen (lua_State *L);
LUALIB_API void lua_iolibopen (lua_State *L);
LUALIB_API void lua_strlibopen (lua_State *L);
LUALIB_API void lua_mathlibopen (lua_State *L);
LUALIB_API void lua_dblibopen (lua_State *L);



/*
** `private' part
*/

/* macro to `unsign' a character */
#define uchar(c)	((unsigned char)(c))

/* integer type to hold the result of fgetc */
typedef int l_charint;

#endif
