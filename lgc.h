/*
** $Id: lgc.h,v 1.5 1999/08/16 20:52:00 roberto Exp roberto $
** Garbage Collector
** See Copyright Notice in lua.h
*/

#ifndef lgc_h
#define lgc_h


#include "lobject.h"


void luaC_checkGC (void);
void luaC_collect (int all);


#endif
