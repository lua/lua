/*
** $Id: ldebug.h,v 1.2 2000/06/28 20:20:36 roberto Exp roberto $
** Auxiliary functions from Debug Interface module
** See Copyright Notice in lua.h
*/

#ifndef ldebug_h
#define ldebug_h


#include "lstate.h"
#include "luadebug.h"


void luaG_callerror (lua_State *L, StkId func);
void luaG_indexerror (lua_State *L, StkId t);
int luaG_getline (int *lineinfo, int pc, int refline, int *refi);


#endif
