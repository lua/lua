/*
** $Id: lfunc.h,v 1.6 1999/08/16 20:52:00 roberto Exp roberto $
** Lua Function structures
** See Copyright Notice in lua.h
*/

#ifndef lfunc_h
#define lfunc_h


#include "lobject.h"



TProtoFunc *luaF_newproto (void);
Closure *luaF_newclosure (int nelems);
void luaF_freeproto (TProtoFunc *f);
void luaF_freeclosure (Closure *c);

const char *luaF_getlocalname (TProtoFunc *func, int local_number, int line);


#endif
