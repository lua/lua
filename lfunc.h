/*
** $Id: lfunc.h,v 1.10 1999/12/27 17:33:22 roberto Exp roberto $
** Auxiliary functions to manipulate prototypes and closures
** See Copyright Notice in lua.h
*/

#ifndef lfunc_h
#define lfunc_h


#include "lobject.h"



Proto *luaF_newproto (lua_State *L);
Closure *luaF_newclosure (lua_State *L, int nelems);
void luaF_freeproto (lua_State *L, Proto *f);
void luaF_freeclosure (lua_State *L, Closure *c);

const char *luaF_getlocalname (const Proto *func,
                               int local_number, int line);


#endif
