/*
** $Id: ltable.h,v 1.3 1997/10/18 16:29:15 roberto Exp roberto $
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

Hash *luaH_new (int nhash);
void luaH_callIM (Hash *l);
void luaH_free (Hash *frees);
TObject *luaH_get (Hash *t, TObject *ref);
TObject *luaH_set (Hash *t, TObject *ref);
Node *luaH_next (TObject *o, TObject *r);

#endif
