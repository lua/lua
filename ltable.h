/*
** $Id: ltable.h,v 1.29 2001/01/26 14:16:35 roberto Exp roberto $
** Lua tables (hash)
** See Copyright Notice in lua.h
*/

#ifndef ltable_h
#define ltable_h

#include "lobject.h"


#define node(t,i)	(&(t)->node[i])
#define key(n)		(&(n)->key)
#define val(n)		(&(n)->val)

#define luaH_get(t,k)	luaH_set(NULL,t,k)
#define luaH_getnum(t,k)	luaH_setnum(NULL,t,k)
#define luaH_getstr(t,k)	luaH_setstr(NULL,t,k)

Hash *luaH_new (lua_State *L, int nhash);
void luaH_free (lua_State *L, Hash *t);
TObject *luaH_set (lua_State *L, Hash *t, const TObject *key);
Node *luaH_next (lua_State *L, Hash *t, const TObject *r);
int luaH_nexti (Hash *t, int i);
TObject *luaH_setnum (lua_State *L, Hash *t, lua_Number key);
TObject *luaH_setstr (lua_State *L, Hash *t, TString *key);

/* exported only for debugging */
Node *luaH_mainposition (const Hash *t, const TObject *key);


#endif
