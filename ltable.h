/*
** $Id: ltable.h,v 1.9 1999/01/25 12:30:11 roberto Exp roberto $
** Lua tables (hash)
** See Copyright Notice in lua.h
*/

#ifndef ltable_h
#define ltable_h

#include "lobject.h"


#define node(t,i)       (&(t)->node[i])
#define ref(n)		(&(n)->ref)
#define val(n)		(&(n)->val)
#define nhash(t)	((t)->nhash)

#define luaH_get(t,ref)	(val(luaH_present((t), (ref))))
#define luaH_move(t,from,to)	(luaH_setint(t, to, luaH_getint(t, from)))

Hash *luaH_new (int nhash);
void luaH_free (Hash *frees);
Node *luaH_present (Hash *t, TObject *key);
void luaH_set (Hash *t, TObject *ref, TObject *val);
Node *luaH_next (Hash *t, TObject *r);
void luaH_setint (Hash *t, int ref, TObject *val);
TObject *luaH_getint (Hash *t, int ref);


#endif
