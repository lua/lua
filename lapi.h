/*
** $Id: lapi.h,v 1.7 1999/09/21 16:10:13 roberto Exp roberto $
** Auxiliary functions from Lua API
** See Copyright Notice in lua.h
*/

#ifndef lapi_h
#define lapi_h


#include "lua.h"
#include "lobject.h"


TObject *luaA_Address (lua_Object o);
void luaA_pushobject (const TObject *o);
GlobalVar *luaA_nextvar (TaggedString *g);
int luaA_next (const Hash *t, int i);
lua_Object luaA_putObjectOnTop (void);

#endif
