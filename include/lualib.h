/*
** $Id: lualib.h,v 1.6 1999/05/05 19:23:11 roberto Exp $
** Lua standard libraries
** See Copyright Notice in lua.h
*/


#ifndef lualib_h
#define lualib_h

#include "lua.h"

void lua_iolibopen   (void);
void lua_strlibopen  (void);
void lua_mathlibopen (void);
void lua_dblibopen   (void);


void lua_userinit (void);


/* To keep compatibility with old versions */

#define iolib_open	lua_iolibopen
#define strlib_open	lua_strlibopen
#define mathlib_open	lua_mathlibopen



/* Auxiliary functions (private) */

char *luaI_classend (char *p);
int luaI_singlematch (int c, char *p, char *ep);

#endif

