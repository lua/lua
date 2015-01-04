/*
** $Id: ltable.h,v 1.44 2003/03/18 12:50:04 roberto Exp $
** Lua tables (hash)
** See Copyright Notice in lua.h
*/

#ifndef ltable_h
#define ltable_h

#include "lobject.h"


#define gnode(t,i)	(&(t)->node[i])
#define gkey(n)		(&(n)->i_key)
#define gval(n)		(&(n)->i_val)


const TObject *luaH_getnum (Table *t, int key);
TObject *luaH_setnum (lua_State *L, Table *t, int key);
const TObject *luaH_getstr (Table *t, TString *key);
const TObject *luaH_get (Table *t, const TObject *key);
TObject *luaH_set (lua_State *L, Table *t, const TObject *key);
Table *luaH_new (lua_State *L, int narray, int lnhash);
void luaH_free (lua_State *L, Table *t);
int luaH_next (lua_State *L, Table *t, StkId key);

/* exported only for debugging */
Node *luaH_mainposition (const Table *t, const TObject *key);


#endif
