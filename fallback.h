/*
** $Id: fallback.h,v 1.2 1994/11/08 19:56:39 roberto Exp roberto $
*/
 
#ifndef fallback_h
#define fallback_h

#include "opcode.h"

extern struct FB {
  char *kind;
  Object function;
} luaI_fallBacks[];

#define FB_ERROR  0
#define FB_INDEX  1
#define FB_GETTABLE  2
#define FB_ARITH  3
#define FB_ORDER  4
#define FB_CONCAT  5
#define FB_UNMINUS  6
#define FB_SETTABLE  7

void luaI_setfallback (void);
void luaI_errorFB (void);
void luaI_indexFB (void);
void luaI_gettableFB (void);
void luaI_arithFB (void);
void luaI_concatFB (void);
void luaI_orderFB (void);
Object *luaI_getlocked (int ref);
void luaI_travlock (void (*fn)(Object *));

#endif

