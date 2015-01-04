/*
** $Id: lfunc.h,v 1.5 1997/12/15 16:17:20 roberto Exp $
** Lua Function structures
** See Copyright Notice in lua.h
*/

#ifndef lfunc_h
#define lfunc_h


#include "lobject.h"



TProtoFunc *luaF_newproto (void);
Closure *luaF_newclosure (int nelems);
void luaF_freeproto (TProtoFunc *l);
void luaF_freeclosure (Closure *l);

char *luaF_getlocalname (TProtoFunc *func, int local_number, int line);


#endif
