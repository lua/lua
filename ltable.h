/*
** $Id: ltable.h,v 1.30 2001/01/29 13:02:20 roberto Exp roberto $
** Lua tables (hash)
** See Copyright Notice in lua.h
*/

#ifndef ltable_h
#define ltable_h

#include "lobject.h"


#define node(_t,_i)	(&(_t)->node[_i])
#define val(_n)		(&(_n)->val)

#define ttype_key(_n)		((_n)->key_tt)
#define nvalue_key(_n)		((_n)->key_value.n)
#define tsvalue_key(_n)		((TString *)(_n)->key_value.v)
#define setkey2obj(_o,_k) \
  ((_o)->tt = ttype_key(_k), (_o)->value = (_k)->key_value)
#define setobj2key(_k,_o) \
  (ttype_key(_k) = (_o)->tt, (_k)->key_value = (_o)->value)

#define luaH_get(_t,_k)	luaH_set(NULL,_t,_k)
#define luaH_getnum(_t,_k)	luaH_setnum(NULL,_t,_k)
#define luaH_getstr(_t,_k)	luaH_setstr(NULL,_t,_k)

Hash *luaH_new (lua_State *L, int nhash);
void luaH_free (lua_State *L, Hash *t);
TObject *luaH_set (lua_State *L, Hash *t, const TObject *key);
Node *luaH_next (lua_State *L, Hash *t, const TObject *r);
int luaH_nexti (Hash *t, int i);
TObject *luaH_setnum (lua_State *L, Hash *t, lua_Number key);
TObject *luaH_setstr (lua_State *L, Hash *t, TString *key);

/* exported only for debugging */
Node *luaH_mainposition (const Hash *t, const Node *n);


#endif
