/*
** $Id: ltable.h,v 1.34 2001/07/05 20:31:14 roberto Exp roberto $
** Lua tables (hash)
** See Copyright Notice in lua.h
*/

#ifndef ltable_h
#define ltable_h

#include "lobject.h"


#define node(_t,_i)	(&(_t)->node[_i])
#define key(_n)		(&(_n)->key)
#define val(_n)		(&(_n)->val)

#define settableval(p,v)	setobj((TObject *)p, v)


const TObject *luaH_getnum (Hash *t, int key);
void luaH_setnum (lua_State *L, Hash *t, int key, const TObject *val);
const TObject *luaH_getstr (Hash *t, TString *key);
void luaH_setstr (lua_State *L, Hash *t, TString *key, const TObject *val);
const TObject *luaH_get (Hash *t, const TObject *key);
void luaH_set (lua_State *L, Hash *t, const TObject *key, const TObject *val);
Hash *luaH_new (lua_State *L, int nhash);
void luaH_free (lua_State *L, Hash *t);
Node *luaH_next (lua_State *L, Hash *t, const TObject *r);
int luaH_nexti (Hash *t, int i);

/* exported only for debugging */
Node *luaH_mainposition (const Hash *t, const TObject *key);


#endif
