/*
** $Id: fallback.h,v 1.1 1994/11/07 15:20:56 roberto Exp roberto $
*/
 
#ifndef fallback_h
#define fallback_h

#include "opcode.h"

void luaI_errorFB (void);
void luaI_indexFB (void);
void luaI_gettableFB (void);
void luaI_arithFB (void);
void luaI_concatFB (void);
void luaI_orderFB (void);
Object *luaI_getlocked (int ref);
void luaI_travlock (void (*fn)(Object *));

#endif

