/*
** $Id: lstring.h,v 1.39 2004/08/24 20:12:06 roberto Exp $
** String table (keep all strings handled by Lua)
** See Copyright Notice in lua.h
*/

#ifndef lstring_h
#define lstring_h


#include "lgc.h"
#include "lobject.h"
#include "lstate.h"


#define sizestring(l)	(cast(lu_mem, sizeof(union TString))+ \
                         (cast(lu_mem, l)+1)*sizeof(char))

#define sizeudata(l)	(cast(lu_mem, sizeof(union Udata))+(l))

#define luaS_new(L, s)	(luaS_newlstr(L, s, strlen(s)))
#define luaS_newliteral(L, s)	(luaS_newlstr(L, "" s, \
                                 (sizeof(s)/sizeof(char))-1))

#define luaS_fix(s)	setbit((s)->tsv.marked, FIXEDBIT)

void luaS_resize (lua_State *L, int newsize);
Udata *luaS_newudata (lua_State *L, size_t s);
TString *luaS_newlstr (lua_State *L, const char *str, size_t l);


#endif
