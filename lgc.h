/*
** $Id: lgc.h,v 1.4 1997/12/01 20:31:25 roberto Exp roberto $
** Garbage Collector
** See Copyright Notice in lua.h
*/

#ifndef lgc_h
#define lgc_h


#include "lobject.h"


void luaC_checkGC (void);
const TObject *luaC_getref (int ref);
int luaC_ref (const TObject *o, int lock);
void luaC_hashcallIM (Hash *l);
void luaC_strcallIM (TaggedString *l);


#endif
