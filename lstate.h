/*
** $Id: lstate.h,v 1.37 2000/08/28 17:57:04 roberto Exp roberto $
** Global State
** See Copyright Notice in lua.h
*/

#ifndef lstate_h
#define lstate_h

#include <setjmp.h>

#include "lobject.h"
#include "lua.h"
#include "luadebug.h"



typedef TObject *StkId;  /* index to stack elements */


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


/*
** chain list of long jumps
*/
struct lua_longjmp {
  jmp_buf b;
  struct lua_longjmp *previous;
  volatile int status;  /* error code */
  StkId base;
};


typedef struct stringtable {
  int size;
  lint32 nuse;  /* number of elements */
  TString **hash;
} stringtable;



struct lua_State {
  /* thread-specific state */
  StkId top;  /* first free slot in the stack */
  StkId stack;  /* stack base */
  StkId stack_last;  /* last free slot in the stack */
  int stacksize;
  StkId Cbase;  /* base for current C function */
  struct lua_longjmp *errorJmp;  /* current error recover point */
  char *Mbuffer;  /* global buffer */
  size_t Mbuffsize;  /* size of Mbuffer */
  /* global state */
  Proto *rootproto;  /* list of all prototypes */
  Closure *rootcl;  /* list of all closures */
  Hash *roottable;  /* list of all tables */
  stringtable strt;  /* hash table for strings */
  stringtable udt;   /* hash table for udata */
  Hash *gt;  /* table for globals */
  struct IM *IMtable;  /* table for tag methods */
  int last_tag;  /* last used tag in IMtable */
  struct Ref *refArray;  /* locked objects */
  int refSize;  /* size of refArray */
  int refFree;  /* list of free positions in refArray */
  unsigned long GCthreshold;
  unsigned long nblocks;  /* number of `blocks' currently allocated */
  lua_Hook callhook;
  lua_Hook linehook;
  int allowhooks;
};


#endif

