/*
** $Id: ldo.h,v 1.31 2001/02/23 17:17:25 roberto Exp roberto $
** Stack and Call structure of Lua
** See Copyright Notice in lua.h
*/

#ifndef ldo_h
#define ldo_h


#include "lobject.h"
#include "lstate.h"


/*
** macro to increment stack top.
** There must be always an empty slot at the L->stack.top
*/
#define incr_top {if (L->top == L->stack_last) luaD_checkstack(L, 1); L->top++;}


void luaD_init (lua_State *L, int stacksize);
void luaD_adjusttop (lua_State *L, StkId base, int extra);
void luaD_lineHook (lua_State *L, int line, lua_Hook linehook);
void luaD_call (lua_State *L, StkId func, int nResults);
void luaD_checkstack (lua_State *L, int n);

void luaD_error (lua_State *L, const l_char *s);
void luaD_breakrun (lua_State *L, int errcode);
int luaD_runprotected (lua_State *L, void (*f)(lua_State *, void *), void *ud);


#endif
