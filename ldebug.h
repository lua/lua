/*
** $Id: $
** Auxiliary functions from Debug Interface module
** See Copyright Notice in lua.h
*/

#ifndef ldebug_h
#define ldebug_h


#include "lobject.h"
#include "luadebug.h"


void luaG_callerror (lua_State *L, TObject *func);
void luaG_indexerror (lua_State *L, TObject *t);


#endif
