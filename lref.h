/*
** $Id: lref.h,v 1.1 1999/10/04 17:50:24 roberto Exp roberto $
** REF mechanism
** See Copyright Notice in lua.h
*/

#ifndef lref_h
#define lref_h

#include "lobject.h"


#define NONEXT          -1      /* to end the free list */
#define HOLD            -2
#define COLLECTED       -3
#define LOCK            -4


struct ref {
  TObject o;
  int st;  /* can be LOCK, HOLD, COLLECTED, or next (for free list) */
};


int luaR_ref (const TObject *o, int lock);
const TObject *luaR_getref (int ref);
void luaR_invalidaterefs (void);


#endif
