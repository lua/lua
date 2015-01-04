/*
** $Id: lapi.h,v 1.4 1999/02/23 14:57:28 roberto Exp $
** Auxiliary functions from Lua API
** See Copyright Notice in lua.h
*/

#ifndef lapi_h
#define lapi_h


#include "lua.h"
#include "lobject.h"


TObject *luaA_Address (lua_Object o);
void luaA_pushobject (TObject *o);
void luaA_packresults (void);
int luaA_passresults (void);
TaggedString *luaA_nextvar (TaggedString *g);
int luaA_next (Hash *t, int i);

#endif
