/*
** $Id: lref.h,v 1.4 1999/12/14 18:33:29 roberto Exp roberto $
** reference mechanism
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


void luaR_invalidaterefs (lua_State *L);

#endif
