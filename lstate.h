/*
** $Id: lstate.h,v 1.1 2001/11/29 22:14:34 rieru Exp rieru $
** Global State
** See Copyright Notice in lua.h
*/

#ifndef lstate_h
#define lstate_h

#include "lua.h"

#include "lobject.h"
#include "ltm.h"
#include "luadebug.h"


/*
** macros for thread syncronization inside Lua core machine:
** all accesses to the global state and to global objects are syncronized.
** Because threads can read the stack of other threads
** (when running garbage collection),
** a thread must also syncronize any write-access to its own stack.
** Unsyncronized accesses are allowed only when reading its own stack,
** or when reading immutable fields from global objects
** (such as string values and udata values). 
*/
#ifndef lua_lock
#define lua_lock(L)	((void) 0)
#endif

#ifndef lua_unlock
#define lua_unlock(L)	((void) 0)
#endif

/*
** macro to allow the inclusion of user information in Lua state
*/
#ifndef LUA_USERSTATE
#define LUA_USERSTATE
#endif



struct lua_longjmp;  /* defined in ldo.c */



/*
** reserve init of stack to store some global values
*/

/* default event table (both for tables and udata) */
#define defaultet(L)	(L->stack)

/* table of globals */
#define gt(L)	(L->stack + 1)

/* registry */
#define registry(L)	(L->stack + 2)

#define RESERVED_STACK_PREFIX	3



typedef struct stringtable {
  int size;
  ls_nstr nuse;  /* number of elements */
  TString **hash;
} stringtable;


/*
** `global state', shared by all threads of this state
*/
typedef struct global_State {
  void *Mbuffer;  /* global buffer */
  size_t Mbuffsize;  /* size of Mbuffer */
  stringtable strt;  /* hash table for strings */
  lu_mem GCthreshold;
  lu_mem nblocks;  /* number of `bytes' currently allocated */
  Proto *rootproto;  /* list of all prototypes */
  Closure *rootcl;  /* list of all closures */
  Table *roottable;  /* list of all tables */
  UpVal *rootupval;  /* list of closed up values */
  Udata *rootudata;   /* list of all userdata */
  Udata *tmudata;  /* list of userdata to be GC */
  TString *tmname[TM_N];  /* array with tag-method names */
} global_State;


/*
** `per thread' state
*/
struct lua_State {
  LUA_USERSTATE
  StkId top;  /* first free slot in the stack */
  CallInfo *ci;  /* call info for current function */
  StkId stack_last;  /* last free slot in the stack */
  StkId stack;  /* stack base */
  int stacksize;
  global_State *_G;
  lua_Hook callhook;
  lua_Hook linehook;
  int allowhooks;
  struct lua_longjmp *errorJmp;  /* current error recover point */
  UpVal *openupval;  /* list of open upvalues in this stack */
  lua_State *next;  /* circular double linked list of states */
  lua_State *previous;
  CallInfo basefunc;
};


#define G(L)	(L->_G)


#endif

