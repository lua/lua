/*
** $Id: lstate.h,v 1.34 2000/05/24 13:54:49 roberto Exp roberto $
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
** chain list of long jumps
*/
struct lua_longjmp {
  jmp_buf b;
  struct lua_longjmp *previous;
  volatile int status;  /* error code */
  StkId base;
  int numCblocks;
};


/*
** stack layout for C point of view:
** [lua2C, lua2C+num) - `array' lua2C
** [lua2C+num, base)  - space for extra lua_Objects (limbo)
** [base, L->top)     - `stack' C2Lua
*/
struct C_Lua_Stack {
  StkId base;
  StkId lua2C;
  int num;
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
  struct C_Lua_Stack Cstack;  /* C2lua struct */
  struct lua_longjmp *errorJmp;  /* current error recover point */
  char *Mbuffer;  /* global buffer */
  size_t Mbuffbase;  /* current first position of Mbuffer */
  size_t Mbuffsize;  /* size of Mbuffer */
  size_t Mbuffnext;  /* next position to fill in Mbuffer */
  struct C_Lua_Stack *Cblocks;
  int numCblocks;  /* number of nested Cblocks */
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
  int debug;
  lua_Hook callhook;
  lua_Hook linehook;
  int allowhooks;
};


#endif

