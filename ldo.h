/*
** $Id: ldo.h,v 1.44 2002/05/01 20:40:42 roberto Exp roberto $
** Stack and Call structure of Lua
** See Copyright Notice in lua.h
*/

#ifndef ldo_h
#define ldo_h


#include "lobject.h"
#include "lstate.h"
#include "lzio.h"


/*
** macro to increment stack top.
** There must be always an empty slot at the L->stack.top
*/
#define incr_top(L) \
	{if (L->top >= L->stack_last) luaD_growstack(L, 1); L->top++;}

#define luaD_checkstack(L,n)	\
  if ((char *)L->stack_last - (char *)L->top <= (n)*(int)sizeof(TObject)) \
    luaD_growstack(L, n)

#define savestack(L,p)		((char *)(p) - (char *)L->stack)
#define restorestack(L,n)	((TObject *)((char *)L->stack + (n)))


/* type of protected functions, to be ran by `runprotected' */
typedef void (*Pfunc) (lua_State *L, void *v);

int luaD_protectedparser (lua_State *L, ZIO *z, int bin);
void luaD_lineHook (lua_State *L, int line);
StkId luaD_precall (lua_State *L, StkId func);
void luaD_call (lua_State *L, StkId func, int nResults);
int luaD_pcall (lua_State *L, int nargs, int nresults, const TObject *err);
void luaD_poscall (lua_State *L, int wanted, StkId firstResult);
void luaD_reallocCI (lua_State *L, int newsize);
void luaD_reallocstack (lua_State *L, int newsize);
void luaD_growstack (lua_State *L, int n);

void luaD_error (lua_State *L, const char *s, int errcode);
void luaD_errorobj (lua_State *L, const TObject *s, int errcode);
int luaD_runprotected (lua_State *L, Pfunc f, TObject *ud);


#endif
