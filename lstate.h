/*
** $Id: lstate.h,v 1.54 2001/03/02 17:27:50 roberto Exp roberto $
** Global State
** See Copyright Notice in lua.h
*/

#ifndef lstate_h
#define lstate_h

#include "lobject.h"
#include "lua.h"
#include "luadebug.h"


/*
** macros that control all entries and exits from Lua core machine
** (mainly for thread syncronization)
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


/*
** marks for Reference array
*/
#define NONEXT          -1      /* to end the free list */
#define HOLD            -2
#define COLLECTED       -3
#define LOCK            -4


struct Ref {
  TObject o;
  int st;  /* can be LOCK, HOLD, COLLECTED, or next (for free list) */
};


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
  Proto *rootproto;  /* list of all prototypes */
  Closure *rootcl;  /* list of all closures */
  Hash *roottable;  /* list of all tables */
  stringtable strt;  /* hash table for strings */
  stringtable udt;   /* hash table for udata */
  Hash *type2tag;  /* hash table from type names to tags */
  struct TM *TMtable;  /* table for tag methods */
  int sizeTM;  /* size of TMtable */
  int ntag;  /* number of tags in TMtable */
  struct Ref *refArray;  /* locked objects */
  int nref;  /* first unused element in refArray */
  int sizeref;  /* size of refArray */
  int refFree;  /* list of free positions in refArray */
  lu_mem GCthreshold;
  lu_mem nblocks;  /* number of `bytes' currently allocated */
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
  lua_State *next;  /* circular double linked list of states */
  lua_State *previous;
  CallInfo basefunc;
};


#define G(L)	(L->G)


#endif

