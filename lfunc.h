/*
** $Id: lfunc.h,v 1.9 1999/11/22 13:12:07 roberto Exp roberto $
** Auxiliary functions to manipulate prototypes and closures
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
