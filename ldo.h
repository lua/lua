/*
** $Id: $
** Stack and Call structure of Lua
** See Copyright Notice in lua.h
*/

#ifndef ldo_h
#define ldo_h


#include "lobject.h"


typedef int StkId;  /* index to luaD_stack.stack elements */

#define MULT_RET        255


extern struct Stack {
  TObject *last;
  TObject *stack;
  TObject *top;
} luaD_stack;


extern struct C_Lua_Stack {
  StkId base;  /* when Lua calls C or C calls Lua, points to */
               /* the first slot after the last parameter. */
  StkId lua2C; /* points to first element of "array" lua2C */
  int num;     /* size of "array" lua2C */
} luaD_Cstack;


extern TObject luaD_errorim;


/*
** macro to increment stack top.
** There must be always an empty slot at the luaD_stack.top
*/
#define incr_top { if (luaD_stack.top >= luaD_stack.last) luaD_checkstack(1); \
                   luaD_stack.top++; }


/* macros to convert from lua_Object to (TObject *) and back */

#define Address(lo)     ((lo)+luaD_stack.stack-1)
#define Ref(st)         ((st)-luaD_stack.stack+1)

void luaD_adjusttop (StkId newtop);
void luaD_openstack (int nelems);
void luaD_lineHook (int line);
void luaD_callHook (StkId base, lua_Type type, int isreturn);
void luaD_call (StkId base, int nResults);
void luaD_callTM (TObject *f, int nParams, int nResults);
int luaD_protectedrun (int nResults);
void luaD_gcIM (TObject *o);
void luaD_travstack (int (*fn)(TObject *));
void luaD_checkstack (int n);


#endif
