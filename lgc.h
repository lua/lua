/*
** $Id: lgc.h,v 1.10 2001/06/05 19:27:32 roberto Exp roberto $
** Garbage Collector
** See Copyright Notice in lua.h
*/

#ifndef lgc_h
#define lgc_h


#include "lobject.h"


#define luaC_checkGC(L) if (G(L)->nblocks >= G(L)->GCthreshold) \
			  luaC_collectgarbage(L)


void luaC_collectudata (lua_State *L);
void luaC_callgcTMudata (lua_State *L);
void luaC_collect (lua_State *L, int all);
void luaC_collectgarbage (lua_State *L);


#endif
