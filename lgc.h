/*
** $Id: lgc.h,v 1.1 1997/09/16 19:25:59 roberto Exp roberto $
** Garbage Collector
** See Copyright Notice in lua.h
*/

#ifndef lgc_h
#define lgc_h


#include "lobject.h"


extern unsigned long luaC_threshold;

void luaC_checkGC (void);
TObject* luaC_getref (int ref);
int luaC_ref (TObject *o, int lock);


#endif
