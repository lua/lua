/*
** $Id: ltable.h,v 1.20 2000/05/08 19:32:53 roberto Exp roberto $
** Lua tables (hash)
** See Copyright Notice in lua.h
*/

#ifndef ltable_h
#define ltable_h

#include "lobject.h"


#define node(t,i)	(&(t)->node[i])
#define key(n)		(&(n)->key)
#define val(n)		(&(n)->val)

#define luaH_move(L, t,from,to)	(luaH_setint(L, t, to, luaH_getnum(t, from)))

Hash *luaH_new (lua_State *L, int nhash);
void luaH_free (lua_State *L, Hash *t);
const TObject *luaH_get (lua_State *L, const Hash *t, const TObject *key);
const TObject *luaH_getnum (const Hash *t, Number key);
const TObject *luaH_getstr (const Hash *t, TString *key);
void luaH_remove (Hash *t, TObject *key);
void luaH_set (lua_State *L, Hash *t, const TObject *key, const TObject *val);
int luaH_pos (lua_State *L, const Hash *t, const TObject *r);
void luaH_setint (lua_State *L, Hash *t, int key, const TObject *val);
unsigned long luaH_hash (lua_State *L, const TObject *key);
const TObject *luaH_getglobal (lua_State *L, const char *name);

/* exported only for debugging */
Node *luaH_mainposition (const Hash *t, const TObject *key);


#endif
