/*
** $Id: lgc.h,v 1.2 1997/10/23 16:26:37 roberto Exp roberto $
** Garbage Collector
** See Copyright Notice in lua.h
*/

#ifndef lgc_h
#define lgc_h


#include "lobject.h"


void luaC_checkGC (void);
TObject* luaC_getref (int ref);
int luaC_ref (TObject *o, int lock);


#endif
