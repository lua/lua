/*
** $Id: lparser.h,v 1.40 2002/03/14 18:01:52 roberto Exp roberto $
** Lua Parser
** See Copyright Notice in lua.h
*/

#ifndef lparser_h
#define lparser_h

#include "llimits.h"
#include "lobject.h"
#include "ltable.h"
#include "lzio.h"


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
  VGLOBAL,	/* info = index of table; aux = index of global name in `k' */
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


/* describe declared variables */
typedef struct vardesc {
  int i;  /* if local, its index in `locvars';
             if global, its name index in `k' */
  lu_byte k;
  lu_byte level;  /* if local, stack level;
                     if global, corresponding local (NO_REG for free globals) */
} vardesc;


struct BlockCnt;  /* defined in lparser.c */

/* state needed to generate code for a given function */
typedef struct FuncState {
  Proto *f;  /* current function header */
  Table *h;  /* table to find (and reuse) elements in `k' */
  struct FuncState *prev;  /* enclosing function */
  struct LexState *ls;  /* lexical state */
  struct lua_State *L;  /* copy of the Lua state */
  struct BlockCnt *bl;  /* chain of current blocks */
  int pc;  /* next position to code (equivalent to `ncode') */
  int lasttarget;   /* `pc' of last `jump target' */
  int jlt;  /* list of jumps to `lasttarget' */
  int freereg;  /* first free register */
  int defaultglob;  /* where to look for non-declared globals */
  int nk;  /* number of elements in `k' */
  int np;  /* number of elements in `p' */
  int nlocvars;  /* number of elements in `locvars' */
  int nactloc;  /* number of active local variables */
  int nactvar;  /* number of elements in array `actvar' */
  expdesc upvalues[MAXUPVALUES];  /* upvalues */
  vardesc actvar[MAXVARS];  /* declared-variable stack */
} FuncState;


Proto *luaY_parser (lua_State *L, ZIO *z);


#endif
