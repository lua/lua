/*
** $Id: lparser.h,v 1.34 2001/08/27 15:16:28 roberto Exp $
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
  VNUMBER,	/* n = value */
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
  union {
    struct {
      int info, aux;
    } i;
    lua_Number n;
  } u;
  int t;  /* patch list of `exit when true' */
  int f;  /* patch list of `exit when false' */
} expdesc;


/* state needed to generate code for a given function */
typedef struct FuncState {
  Proto *f;  /* current function header */
  struct FuncState *prev;  /* enclosing function */
  struct LexState *ls;  /* lexical state */
  struct lua_State *L;  /* copy of the Lua state */
  int pc;  /* next position to code (equivalent to `ncode') */
  int lasttarget;   /* `pc' of last `jump target' */
  int jlt;  /* list of jumps to `lasttarget' */
  int freereg;  /* first free register */
  int nk;  /* number of elements in `k' */
  Hash *h;  /* table to find (and reuse) elements in `k' */
  int np;  /* number of elements in `p' */
  int nlineinfo;  /* number of elements in `lineinfo' */
  int nlocvars;  /* number of elements in `locvars' */
  int nactloc;  /* number of active local variables */
  int lastline;  /* line where last `lineinfo' was generated */
  struct Breaklabel *bl;  /* chain of breakable blocks */
  expdesc upvalues[MAXUPVALUES];  /* upvalues */
  int actloc[MAXLOCALS];  /* local-variable stack (indices to locvars) */
  unsigned int wasup[words2bits(MAXLOCALS)];  /* bit array to mark whether a
                             local variable was used as upvalue at some level */
} FuncState;


Proto *luaY_parser (lua_State *L, ZIO *z);


#endif
