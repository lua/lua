/*
** $Id: fallback.h,v 1.13 1996/04/25 14:10:00 roberto Exp roberto $
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

#define FB_GETTABLE  0
#define FB_ARITH  1
#define FB_ORDER  2
#define FB_CONCAT  3
#define FB_SETTABLE  4
#define FB_GC 5
#define FB_FUNCTION 6
#define FB_GETGLOBAL 7
#define FB_INDEX  8
#define FB_ERROR  9
#define FB_N 10

void luaI_setfallback (void);
int luaI_ref (Object *object, int lock);
Object *luaI_getref (int ref);
void luaI_travlock (int (*fn)(Object *));
void luaI_invalidaterefs (void);
char *luaI_travfallbacks (int (*fn)(Object *));

void luaI_settag (int tag, Object *o);
Object *luaI_getim (int tag, int event);
int luaI_tag (Object *o);
void luaI_setintmethod (void);

#endif

