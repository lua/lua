/*
** $Id: lualib.h,v 1.11 2000/09/05 19:33:32 roberto Exp roberto $
** Lua standard libraries
** See Copyright Notice in lua.h
*/


#ifndef lualib_h
#define lualib_h

#include "lua.h"


#define LUA_ALERT               "_ALERT"

void lua_baselibopen (lua_State *L);
void lua_iolibopen (lua_State *L);
void lua_strlibopen (lua_State *L);
void lua_mathlibopen (lua_State *L);
void lua_dblibopen (lua_State *L);



/* Auxiliary functions (private) */

const char *luaI_classend (lua_State *L, const char *p);
int luaI_singlematch (int c, const char *p, const char *ep);

#endif
