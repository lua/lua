/*
** $Id: fallback.h,v 1.11 1996/01/30 15:25:23 roberto Exp roberto $
*/
 
#ifndef fallback_h
#define fallback_h

#include "lua.h"
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
#define FB_GETGLOBAL 9

void luaI_setfallback (void);
lua_Reference luaI_ref (Object *object, int lock);
Object *luaI_getref (lua_Reference ref);
void luaI_travlock (int (*fn)(Object *));
void luaI_invalidaterefs (void);
char *luaI_travfallbacks (int (*fn)(Object *));

#endif

