/*
** $Id: lualib.h,v 1.16 2001/02/22 17:15:18 roberto Exp roberto $
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

/* macro to control type of literal strings */
#ifndef l_s
#define l_s(x)	x
#endif

/* macro to control type of literal chars */
#ifndef l_c
#define l_c(x)	x
#endif

#endif
