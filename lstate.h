/*
** $Id: lstate.h,v 1.59 2001/09/07 17:39:10 roberto Exp $
** Global State
** See Copyright Notice in lua.h
*/

#ifndef lstate_h
#define lstate_h

#include "lobject.h"
#include "lua.h"
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
struct TM;  /* defined in ltm.h */


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
  Hash *type2tag;  /* hash table from type names to tags */
  Hash *registry;  /* (strong) registry table */
  Hash *weakregistry;  /* weakregistry table */
  struct TM *TMtable;  /* table for tag methods */
  int sizeTM;  /* size of TMtable */
  int ntag;  /* number of tags in TMtable */
  lu_mem GCthreshold;
  lu_mem nblocks;  /* number of `bytes' currently allocated */
  Proto *rootproto;  /* list of all prototypes */
  Closure *rootcl;  /* list of all C closures and closed Lua closures */
  Hash *roottable;  /* list of all tables */
  Udata *rootudata;   /* list of all userdata */
  UpVal *rootupval;  /* list of all up values */
} global_State;


/*
** `per thread' state
*/
struct lua_State {
  LUA_USERSTATE
  StkId top;  /* first free slot in the stack */
  CallInfo *ci;  /* call info for current function */
  StkId stack_last;  /* last free slot in the stack */
  Hash *gt;  /* table for globals */
  global_State *G;
  StkId stack;  /* stack base */
  int stacksize;
  lua_Hook callhook;
  lua_Hook linehook;
  int allowhooks;
  struct lua_longjmp *errorJmp;  /* current error recover point */
  Closure *opencl;  /* list of closures still pointing to this stack */
  lua_State *next;  /* circular double linked list of states */
  lua_State *previous;
  CallInfo basefunc;
};


#define G(L)	(L->G)


#endif

