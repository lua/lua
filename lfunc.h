/*
** $Id: $
** Lua Function structures
** See Copyright Notice in lua.h
*/

#ifndef lfunc_h
#define lfunc_h


#include "lobject.h"


extern TProtoFunc *luaF_root;
extern Closure *luaF_rootcl;


TProtoFunc *luaF_newproto (void);
Closure *luaF_newclosure (int nelems);
void luaF_freeproto (TProtoFunc *l);
void luaF_freeclosure (Closure *l);

char *luaF_getlocalname (TProtoFunc *func, int local_number, int line);


#endif
