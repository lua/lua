/*
** $Id: lgc.h,v 1.3 1997/11/19 17:29:23 roberto Exp roberto $
** Garbage Collector
** See Copyright Notice in lua.h
*/

#ifndef lgc_h
#define lgc_h


#include "lobject.h"


void luaC_checkGC (void);
TObject* luaC_getref (int ref);
int luaC_ref (TObject *o, int lock);
void luaC_hashcallIM (Hash *l);
void luaC_strcallIM (TaggedString *l);


#endif
