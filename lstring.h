/*
** $Id: lstring.h,v 1.13 1999/11/22 13:12:07 roberto Exp roberto $
** String table (keep all strings handled by Lua)
** See Copyright Notice in lua.h
*/

#ifndef lstring_h
#define lstring_h


#include "lobject.h"
#include "lstate.h"


#define NUM_HASHSTR     31  /* a prime not in array `dimensions' */
#define NUM_HASHUDATA   31  /* idem */
#define NUM_HASHS (NUM_HASHSTR+NUM_HASHUDATA)


/*
** any taggedstring with mark>=FIXMARK is never collected.
** Marks>=RESERVEDMARK are used to identify reserved words.
*/
#define FIXMARK		2
#define RESERVEDMARK	3


void luaS_init (lua_State *L);
void luaS_resize (lua_State *L, stringtable *tb, int newsize);
TaggedString *luaS_createudata (lua_State *L, void *udata, int tag);
void luaS_freeall (lua_State *L);
void luaS_free (lua_State *L, TaggedString *ts);
TaggedString *luaS_newlstr (lua_State *L, const char *str, long l);
TaggedString *luaS_new (lua_State *L, const char *str);
TaggedString *luaS_newfixedstring (lua_State *L, const char *str);
GlobalVar *luaS_assertglobal (lua_State *L, TaggedString *ts);
GlobalVar *luaS_assertglobalbyname (lua_State *L, const char *name);
int luaS_globaldefined (lua_State *L, const char *name);


#endif
