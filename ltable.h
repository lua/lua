/*
** $Id: ltable.h,v 1.27 2001/01/10 18:56:11 roberto Exp roberto $
** Lua tables (hash)
** See Copyright Notice in lua.h
*/

#ifndef ltable_h
#define ltable_h

#include "lobject.h"


#define node(t,i)	(&(t)->node[i])
#define key(n)		(&(n)->key)
#define val(n)		(&(n)->val)

Hash *luaH_new (lua_State *L, int nhash);
void luaH_free (lua_State *L, Hash *t);
const TObject *luaH_get (const Hash *t, const TObject *key);
const TObject *luaH_getnum (const Hash *t, lua_Number key);
const TObject *luaH_getstr (const Hash *t, TString *key);
TObject *luaH_set (lua_State *L, Hash *t, const TObject *key);
Node * luaH_next (lua_State *L, const Hash *t, const TObject *r);
TObject *luaH_setnum (lua_State *L, Hash *t, lua_Number key);
TObject *luaH_setstr (lua_State *L, Hash *t, TString *key);

/* exported only for debugging */
Node *luaH_mainposition (const Hash *t, const TObject *key);


#endif
