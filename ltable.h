/*
** $Id: ltable.h,v 1.11 1999/02/23 14:57:28 roberto Exp roberto $
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
Node *luaH_present (const Hash *t, const TObject *key);
void luaH_set (Hash *t, const TObject *ref, const TObject *val);
int luaH_pos (const Hash *t, const TObject *r);
void luaH_setint (Hash *t, int ref, const TObject *val);
TObject *luaH_getint (const Hash *t, int ref);


#endif
