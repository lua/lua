/*
** $Id: lfunc.h,v 1.7 1999/10/04 17:51:04 roberto Exp roberto $
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

const char *luaF_getlocalname (const TProtoFunc *func,
                               int local_number, int line);


#endif
