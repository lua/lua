/*
** $Id: lfunc.h,v 1.8 1999/10/14 19:46:57 roberto Exp roberto $
** Lua Function structures
** See Copyright Notice in lua.h
*/

#ifndef lfunc_h
#define lfunc_h


#include "lobject.h"



TProtoFunc *luaF_newproto (lua_State *L);
Closure *luaF_newclosure (lua_State *L, int nelems);
void luaF_freeproto (lua_State *L, TProtoFunc *f);
void luaF_freeclosure (lua_State *L, Closure *c);

const char *luaF_getlocalname (const TProtoFunc *func,
                               int local_number, int line);


#endif
