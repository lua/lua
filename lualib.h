/*
** $Id: $
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

char *luaI_addchar (int c);
void luaI_emptybuff (void);
void luaI_addquoted (char *s);

int luaI_singlematch (int c, char *p, char **ep);

#endif

