/*
** $Id: ldo.h,v 2.2 2004/05/14 19:25:09 roberto Exp $
** Stack and Call structure of Lua
** See Copyright Notice in lua.h
*/

#ifndef ldo_h
#define ldo_h


#include "lobject.h"
#include "lstate.h"
#include "lzio.h"


/*
** macro to control inclusion of some hard tests on stack reallocation
*/ 
#ifndef HARDSTACKTESTS
#define condhardstacktests(x)	{ /* empty */ }
#else
#define condhardstacktests(x)	x
#endif


#define luaD_checkstack(L,n)	\
  if ((char *)L->stack_last - (char *)L->top <= (n)*(int)sizeof(TValue)) \
    luaD_growstack(L, n); \
  else condhardstacktests(luaD_reallocstack(L, L->stacksize));


#define incr_top(L) {luaD_checkstack(L,1); L->top++;}

#define savestack(L,p)		((char *)(p) - (char *)L->stack)
#define restorestack(L,n)	((TValue *)((char *)L->stack + (n)))

#define saveci(L,p)		((char *)(p) - (char *)L->base_ci)
#define restoreci(L,n)		((CallInfo *)((char *)L->base_ci + (n)))


/* results from luaD_precall */
#define PCRLUA		0	/* initiated a call to a Lua function */
#define PCRC		1	/* did a call to a C function */
#define PCRYIELD	2	/* C funtion yielded */


/* type of protected functions, to be ran by `runprotected' */
typedef void (*Pfunc) (lua_State *L, void *ud);

void luaD_resetprotection (lua_State *L);
int luaD_protectedparser (lua_State *L, ZIO *z, const char *name);
void luaD_callhook (lua_State *L, int event, int line);
int luaD_precall (lua_State *L, StkId func, int nresults);
void luaD_call (lua_State *L, StkId func, int nResults);
int luaD_pcall (lua_State *L, Pfunc func, void *u,
                ptrdiff_t oldtop, ptrdiff_t ef);
void luaD_poscall (lua_State *L, int wanted, StkId firstResult);
void luaD_reallocCI (lua_State *L, int newsize);
void luaD_reallocstack (lua_State *L, int newsize);
void luaD_growstack (lua_State *L, int n);

void luaD_throw (lua_State *L, int errcode);
int luaD_rawrunprotected (lua_State *L, Pfunc f, void *ud);


#endif
