/*
** $Id: lgc.h,v 1.9 2001/02/02 16:23:20 roberto Exp $
** Garbage Collector
** See Copyright Notice in lua.h
*/

#ifndef lgc_h
#define lgc_h


#include "lobject.h"


void luaC_collect (lua_State *L, int all);
void luaC_collectgarbage (lua_State *L);
void luaC_checkGC (lua_State *L);


#endif
