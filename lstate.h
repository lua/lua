/*
** $Id: lstate.h,v 1.26 1999/12/21 18:04:41 roberto Exp roberto $
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
** `jmp_buf' may be an array, so it is better to make sure it has an
** address (and not that it *is* an address...)
*/
struct lua_longjmp {
  jmp_buf b;
};


/*
** stack layout for C point of view:
** [lua2C, lua2C+num) - `array' lua2C
** [lua2C+num, base)  - space for extra lua_Objects
** [base, L->top)     - `stack' C2Lua
*/
struct C_Lua_Stack {
  StkId base;
  StkId lua2C;
  int num;
};


typedef struct stringtable {
  int size;
  int nuse;  /* number of elements (including EMPTYs) */
  TaggedString **hash;
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
  int Mbuffbase;  /* current first position of Mbuffer */
  int Mbuffsize;  /* size of Mbuffer */
  int Mbuffnext;  /* next position to fill in Mbuffer */
  struct C_Lua_Stack *Cblocks;
  int numCblocks;  /* number of nested Cblocks */
  /* global state */
  TProtoFunc *rootproto;  /* list of all prototypes */
  Closure *rootcl;  /* list of all closures */
  Hash *roottable;  /* list of all tables */
  GlobalVar *rootglobal;  /* list of global variables */
  stringtable *string_root;  /* array of hash tables for strings and udata */
  struct IM *IMtable;  /* table for tag methods */
  int last_tag;  /* last used tag in IMtable */
  struct ref *refArray;  /* locked objects */
  int refSize;  /* size of refArray */
  int refFree;  /* list of free positions in refArray */
  unsigned long GCthreshold;
  unsigned long nblocks;  /* number of `blocks' currently allocated */
  int debug;
  lua_CHFunction callhook;
  lua_LHFunction linehook;
  int allowhooks;
};





#endif

