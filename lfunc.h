/*
** $Id: lfunc.h,v 1.2 1997/09/26 16:46:20 roberto Exp roberto $
** Lua Function structures
** See Copyright Notice in lua.h
*/

#ifndef lfunc_h
#define lfunc_h


#include "lobject.h"


extern GCnode luaF_root;
extern GCnode luaF_rootcl;


TProtoFunc *luaF_newproto (void);
Closure *luaF_newclosure (int nelems);
void luaF_freeproto (TProtoFunc *l);
void luaF_freeclosure (Closure *l);
void luaF_simpleclosure (TObject *o);

char *luaF_getlocalname (TProtoFunc *func, int local_number, int line);


#endif
