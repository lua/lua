/*
** $Id: lstring.h,v 1.21 2000/05/24 13:54:49 roberto Exp roberto $
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


#define sizestring(l)	(sizeof(TString)+(lint32)(l)*sizeof(char))
#define gcsizeudata	(sizeof(TString))


void luaS_init (lua_State *L);
void luaS_resize (lua_State *L, stringtable *tb, int newsize);
TString *luaS_createudata (lua_State *L, void *udata, int tag);
void luaS_freeall (lua_State *L);
TString *luaS_newlstr (lua_State *L, const char *str, size_t l);
TString *luaS_new (lua_State *L, const char *str);
TString *luaS_newfixed (lua_State *L, const char *str);


#endif
