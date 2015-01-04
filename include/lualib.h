/*
** $Id: lualib.h,v 1.4 1998/06/19 16:14:09 roberto Exp $
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



/* Auxiliary functions (private) */

int luaI_singlematch (int c, char *p, char **ep);

#endif

