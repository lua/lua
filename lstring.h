/*
** $Id: lstring.h,v 1.25 2000/11/24 17:39:56 roberto Exp roberto $
** String table (keep all strings handled by Lua)
** See Copyright Notice in lua.h
*/

#ifndef lstring_h
#define lstring_h


#include "lobject.h"
#include "lstate.h"



/*
** type equivalent to TString, but with maximum alignment requirements
*/
union L_UTString {
  TString ts;
  union L_Umaxalign dummy;  /* ensures maximum alignment for `local' udata */
};



/*
** any TString with mark>=FIXMARK is never collected.
** Marks>=RESERVEDMARK are used to identify reserved words.
*/
#define FIXMARK		2
#define RESERVEDMARK	3


#define sizestring(l)	((lint32)sizeof(TString) + \
                         ((lint32)(l+1)-TSPACK)*(lint32)sizeof(char))

#define sizeudata(l)	((luint32)sizeof(union L_UTString)+(l))


void luaS_init (lua_State *L);
void luaS_resize (lua_State *L, stringtable *tb, int newsize);
TString *luaS_newudata (lua_State *L, size_t s, void *udata);
TString *luaS_createudata (lua_State *L, void *udata, int tag);
void luaS_freeall (lua_State *L);
TString *luaS_newlstr (lua_State *L, const char *str, size_t l);
TString *luaS_new (lua_State *L, const char *str);
TString *luaS_newfixed (lua_State *L, const char *str);


#endif
