/*
** $Id: lstring.h,v 1.33 2001/06/15 20:36:57 roberto Exp $
** String table (keep all strings handled by Lua)
** See Copyright Notice in lua.h
*/

#ifndef lstring_h
#define lstring_h


#include "lobject.h"
#include "lstate.h"



/*
** any TString with mark>=FIXMARK is never collected.
** Marks>=RESERVEDMARK are used to identify reserved words.
*/
#define FIXMARK		2
#define RESERVEDMARK	3


#define sizestring(l)	(cast(lu_mem, sizeof(union TString))+ \
                         (cast(lu_mem, l)+1)*sizeof(l_char))

#define sizeudata(l)	(cast(lu_mem, sizeof(union Udata))+(l))

#define luaS_new(L, s)	(luaS_newlstr(L, s, strlen(s)))
#define luaS_newliteral(L, s)	(luaS_newlstr(L, l_s("") s, \
                                 (sizeof(s)/sizeof(l_char))-1))

void luaS_resize (lua_State *L, int newsize);
Udata *luaS_newudata (lua_State *L, size_t s);
void luaS_freeall (lua_State *L);
TString *luaS_newlstr (lua_State *L, const l_char *str, size_t l);


#endif
