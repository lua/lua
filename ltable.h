/*
** $Id: ltable.h,v 1.13 1999/10/04 17:51:04 roberto Exp roberto $
** Lua tables (hash)
** See Copyright Notice in lua.h
*/

#ifndef ltable_h
#define ltable_h

#include "lobject.h"


#define node(t,i)       (&(t)->node[i])
#define key(n)		(&(n)->key)
#define val(n)		(&(n)->val)

#define luaH_move(t,from,to)	(luaH_setint(t, to, luaH_getint(t, from)))

Hash *luaH_new (int nhash);
void luaH_free (Hash *t);
const TObject *luaH_get (const Hash *t, const TObject *key);
void luaH_set (Hash *t, const TObject *key, const TObject *val);
int luaH_pos (const Hash *t, const TObject *r);
void luaH_setint (Hash *t, int key, const TObject *val);
const TObject *luaH_getint (const Hash *t, int key);
unsigned long luaH_hash (const TObject *key);  /* exported only for debugging */


#endif
