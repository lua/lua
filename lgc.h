/*
** $Id: lgc.h,v 1.6 1999/10/04 17:51:04 roberto Exp roberto $
** Garbage Collector
** See Copyright Notice in lua.h
*/

#ifndef lgc_h
#define lgc_h


#include "lobject.h"


void luaC_checkGC (lua_State *L);
void luaC_collect (lua_State *L, int all);


#endif
