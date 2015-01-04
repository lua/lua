/*
** $Id: linit.c,v 1.1 1999/01/08 16:49:32 roberto Exp $
** Initialization of libraries for lua.c
** See Copyright Notice in lua.h
*/

#include "lua.h"
#include "lualib.h"


void lua_userinit (void) {
  lua_iolibopen();
  lua_strlibopen();
  lua_mathlibopen();
  lua_dblibopen();
}

