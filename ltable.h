/*
** $Id: ltable.h,v 1.45 2003/08/26 12:04:13 roberto Exp roberto $
** Lua tables (hash)
** See Copyright Notice in lua.h
*/

#ifndef ltable_h
#define ltable_h

#include "lobject.h"


#define gnode(t,i)	(&(t)->node[i])
#define gkey(n)		(&(n)->i_key)
#define gval(n)		(&(n)->i_val)


const TValue *luaH_getnum (Table *t, int key);
TValue *luaH_setnum (lua_State *L, Table *t, int key);
const TValue *luaH_getstr (Table *t, TString *key);
TValue *luaH_setstr (lua_State *L, Table *t, TString *key);
const TValue *luaH_get (Table *t, const TValue *key);
TValue *luaH_set (lua_State *L, Table *t, const TValue *key);
Table *luaH_new (lua_State *L, int narray, int lnhash);
void luaH_free (lua_State *L, Table *t);
int luaH_next (lua_State *L, Table *t, StkId key);

/* exported only for debugging */
Node *luaH_mainposition (const Table *t, const TValue *key);


#endif
