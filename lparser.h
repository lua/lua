/*
** $Id: lparser.h,v 1.1 2001/11/29 22:14:34 rieru Exp rieru $
** Lua Parser
** See Copyright Notice in lua.h
*/

#ifndef lparser_h
#define lparser_h

#include "llimits.h"
#include "lobject.h"
#include "ltable.h"
#include "lzio.h"



/* small implementation of bit arrays */

#define BPW	(CHAR_BIT*sizeof(unsigned int))  /* bits per word */

#define words2bits(b)	(((b)-1)/BPW + 1)

#define setbit(a, b)	((a)[(b)/BPW] |= (1 << (b)%BPW))
#define resetbit(a, b)	((a)[(b)/BPW] &= ~((1 << (b)%BPW)))
#define testbit(a, b)	((a)[(b)/BPW] & (1 << (b)%BPW))


/*
** Expression descriptor
*/

typedef enum {
  VVOID,	/* no value */
  VNIL,
  VTRUE,
  VFALSE,
  VK,		/* info = index of constant in `k' */
  VLOCAL,	/* info = local register */
  VUPVAL,       /* info = index of upvalue in `upvalues' */
  VGLOBAL,	/* info = index of global name in `k' */
  VINDEXED,	/* info = table register; aux = index register (or `k') */
  VRELOCABLE,	/* info = instruction pc */
  VNONRELOC,	/* info = result register */
  VJMP,		/* info = result register */
  VCALL		/* info = result register */
} expkind;

typedef struct expdesc {
  expkind k;
  int info, aux;
  int t;  /* patch list of `exit when true' */
  int f;  /* patch list of `exit when false' */
} expdesc;


/* state needed to generate code for a given function */
typedef struct FuncState {
  Proto *f;  /* current function header */
  Table *h;  /* table to find (and reuse) elements in `k' */
  struct FuncState *prev;  /* enclosing function */
  struct LexState *ls;  /* lexical state */
  struct lua_State *L;  /* copy of the Lua state */
  struct Breaklabel *bl;  /* chain of breakable blocks */
  int pc;  /* next position to code (equivalent to `ncode') */
  int lasttarget;   /* `pc' of last `jump target' */
  int jlt;  /* list of jumps to `lasttarget' */
  int freereg;  /* first free register */
  int nk;  /* number of elements in `k' */
  int np;  /* number of elements in `p' */
  int nlineinfo;  /* number of elements in `lineinfo' */
  int nlocvars;  /* number of elements in `locvars' */
  int nactloc;  /* number of active local variables */
  int lastline;  /* line where last `lineinfo' was generated */
  expdesc upvalues[MAXUPVALUES];  /* upvalues */
  int actloc[MAXLOCALS];  /* local-variable stack (indices to locvars) */
  unsigned int wasup[words2bits(MAXLOCALS)];  /* bit array to mark whether a
                             local variable was used as upvalue at some level */
} FuncState;


Proto *luaY_parser (lua_State *L, ZIO *z);


#endif
