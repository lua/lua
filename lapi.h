/*
** $Id: lapi.h,v 1.5 1999/08/16 20:52:00 roberto Exp roberto $
** Auxiliary functions from Lua API
** See Copyright Notice in lua.h
*/

#ifndef lapi_h
#define lapi_h


#include "lua.h"
#include "lobject.h"


TObject *luaA_Address (lua_Object o);
void luaA_pushobject (const TObject *o);
void luaA_packresults (void);
int luaA_passresults (void);
TaggedString *luaA_nextvar (TaggedString *g);
int luaA_next (const Hash *t, int i);
lua_Object put_luaObjectonTop (void);

#endif
