/*
** $Id: lstring.h,v 1.27 2001/01/10 17:41:50 roberto Exp roberto $
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

#define luaS_new(L, s)	(luaS_newlstr(L, s, strlen(s)))
#define luaS_newliteral(L, s)	(luaS_newlstr(L, "" s, (sizeof(s))-1))

void luaS_init (lua_State *L);
void luaS_resize (lua_State *L, stringtable *tb, int newsize);
TString *luaS_newudata (lua_State *L, size_t s, void *udata);
int luaS_createudata (lua_State *L, void *udata, TObject *o);
void luaS_freeall (lua_State *L);
TString *luaS_newlstr (lua_State *L, const char *str, size_t l);


#endif
