/*
** $Id: ldebug.h,v 1.1 2000/01/14 17:15:44 roberto Exp roberto $
** Auxiliary functions from Debug Interface module
** See Copyright Notice in lua.h
*/

#ifndef ldebug_h
#define ldebug_h


#include "lstate.h"
#include "luadebug.h"


void luaG_callerror (lua_State *L, StkId func);
void luaG_indexerror (lua_State *L, StkId t);


#endif
