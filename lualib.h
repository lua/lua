/*
** $Id: lualib.h,v 1.2 1997/11/26 18:53:45 roberto Exp roberto $
** Lua standard libraries
** See Copyright Notice in lua.h
*/


#ifndef lualib_h
#define lualib_h

#include "lua.h"


void lua_iolibopen   (void);
void lua_strlibopen  (void);
void lua_mathlibopen (void);




/* To keep compatibility with old versions */

#define iolib_open	lua_iolibopen
#define strlib_open	lua_strlibopen
#define mathlib_open	lua_mathlibopen



/* auxiliar functions (private) */

int luaI_singlematch (int c, char *p, char **ep);

#endif

