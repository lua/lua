/*
** $Id: linit.c,v 1.4 2000/06/12 13:52:05 roberto Exp roberto $
** Initialization of libraries for lua.c
** See Copyright Notice in lua.h
*/

#include "lua.h"

#include "lualib.h"


void lua_userinit (void) {
  lua_beginblock();
  lua_iolibopen();
  lua_strlibopen();
  lua_mathlibopen();
  lua_dblibopen();
  lua_endblock();
}

