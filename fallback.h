/*
** $Id: fallback.h,v 1.9 1995/10/09 13:14:29 roberto Exp roberto $
*/
 
#ifndef fallback_h
#define fallback_h

#include "opcode.h"

extern struct FB {
  char *kind;
  Object function;
  int nParams;
  int nResults;
} luaI_fallBacks[];

#define FB_ERROR  0
#define FB_INDEX  1
#define FB_GETTABLE  2
#define FB_ARITH  3
#define FB_ORDER  4
#define FB_CONCAT  5
#define FB_SETTABLE  6
#define FB_GC 7
#define FB_FUNCTION 8

void luaI_setfallback (void);
int luaI_lock (Object *object);
Object *luaI_getlocked (int ref);
void luaI_travlock (int (*fn)(Object *));
char *luaI_travfallbacks (int (*fn)(Object *));

#endif

