/*
** $Id: $
** Garbage Collector
** See Copyright Notice in lua.h
*/

#ifndef lgc_h
#define lgc_h


#include "lobject.h"


extern long luaC_threshold;

void luaC_checkGC (void);
TObject* luaC_getref (int ref);
int luaC_ref (TObject *o, int lock);


#endif
