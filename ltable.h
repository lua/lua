/*
** $Id: ltable.h,v 1.37 2001/10/25 19:14:14 roberto Exp $
** Lua tables (hash)
** See Copyright Notice in lua.h
*/

#ifndef ltable_h
#define ltable_h

#include "lobject.h"


#define node(t,i)	(&(t)->node[i])
#define key(n)		(&(n)->_key)
#define val(n)		(&(n)->_val)

#define settableval(p,v)	setobj(cast(TObject *, p), v)


const TObject *luaH_getnum (Table *t, int key);
void luaH_setnum (lua_State *L, Table *t, int key, const TObject *val);
const TObject *luaH_getstr (Table *t, TString *key);
void luaH_setstr (lua_State *L, Table *t, TString *key, const TObject *val);
const TObject *luaH_get (Table *t, const TObject *key);
void luaH_set (lua_State *L, Table *t, const TObject *key, const TObject *val);
Table *luaH_new (lua_State *L, int narray, int lnhash);
void luaH_free (lua_State *L, Table *t);
int luaH_index (lua_State *L, Table *t, const TObject *key);
int luaH_nexti (Table *t, int i, TObject *where);

/* exported only for debugging */
Node *luaH_mainposition (const Table *t, const TObject *key);


#endif
