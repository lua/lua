/*
** $Id: ldo.h,v 1.6 1999/06/22 20:37:23 roberto Exp $
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
#define incr_top { if (L->stack.top >= L->stack.last) luaD_checkstack(1); \
                   L->stack.top++; }


/* macros to convert from lua_Object to (TObject *) and back */

#define Address(lo)     ((lo)+L->stack.stack-1)
#define Ref(st)         ((st)-L->stack.stack+1)


void luaD_init (void);
void luaD_adjusttop (StkId newtop);
void luaD_openstack (int nelems);
void luaD_lineHook (int line);
void luaD_callHook (StkId base, TProtoFunc *tf, int isreturn);
void luaD_calln (int nArgs, int nResults);
void luaD_callTM (TObject *f, int nParams, int nResults);
int luaD_protectedrun (void);
void luaD_gcIM (TObject *o);
void luaD_travstack (int (*fn)(TObject *));
void luaD_checkstack (int n);


#endif
