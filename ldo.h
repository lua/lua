/*
** $Id: ldo.h,v 1.9 1999/10/14 19:46:57 roberto Exp roberto $
** Stack and Call structure of Lua
** See Copyright Notice in lua.h
*/

#ifndef ldo_h
#define ldo_h


#include "lobject.h"
#include "lstate.h"


#define MULT_RET        255



/*
** macro to increment stack top.
** There must be always an empty slot at the L->stack.top
*/
#define incr_top { if (L->stack.top >= L->stack.last) luaD_checkstack(L, 1); \
                   L->stack.top++; }


/* macros to convert from lua_Object to (TObject *) and back */

#define Address(L, lo)     ((lo)+L->stack.stack-1)
#define Ref(L, st)         ((st)-L->stack.stack+1)


void luaD_init (lua_State *L);
void luaD_adjusttop (lua_State *L, StkId newtop);
void luaD_openstack (lua_State *L, int nelems);
void luaD_lineHook (lua_State *L, int line);
void luaD_callHook (lua_State *L, StkId base, const TProtoFunc *tf, int isreturn);
void luaD_calln (lua_State *L, int nArgs, int nResults);
void luaD_callTM (lua_State *L, const TObject *f, int nParams, int nResults);
int luaD_protectedrun (lua_State *L);
void luaD_gcIM (lua_State *L, const TObject *o);
void luaD_checkstack (lua_State *L, int n);


#endif
