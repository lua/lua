/*
** $Id: lualib.h,v 1.9 2000/06/16 17:22:43 roberto Exp roberto $
** Lua standard libraries
** See Copyright Notice in lua.h
*/


#ifndef lualib_h
#define lualib_h

#include "lua.h"

void lua_iolibopen (lua_State *L);
void lua_strlibopen (lua_State *L);
void lua_mathlibopen (lua_State *L);
void lua_dblibopen (lua_State *L);



/* 
** ===============================================================
** Macros (and functions) for single-state use
** ===============================================================
*/

#ifdef LUA_SINGLESTATE

#define lua_iolibopen()		(lua_iolibopen)(lua_state)
#define lua_strlibopen()	(lua_strlibopen)(lua_state)
#define lua_mathlibopen()	(lua_mathlibopen)(lua_state)
#define lua_dblibopen()		(lua_dblibopen)(lua_state)

/* this function should be used only in single-state mode */
void lua_userinit (void);

#endif



/* Auxiliary functions (private) */

const char *luaI_classend (lua_State *L, const char *p);
int luaI_singlematch (int c, const char *p, const char *ep);

#endif
