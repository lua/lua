/*
** $Id: lparser.h,v 1.65 2010/07/07 16:27:29 roberto Exp roberto $
** Lua Parser
** See Copyright Notice in lua.h
*/

#ifndef lparser_h
#define lparser_h

#include "llimits.h"
#include "lobject.h"
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
  VKNUM,	/* nval = numerical value */
  VNONRELOC,	/* info = result register */
  VLOCAL,	/* info = local register */
  VUPVAL,       /* info = index of upvalue in 'upvalues' */
  VINDEXED,	/* t = table register/upvalue; idx = index R/K */
  VJMP,		/* info = instruction pc */
  VRELOCABLE,	/* info = instruction pc */
  VCALL,	/* info = instruction pc */
  VVARARG	/* info = instruction pc */
} expkind;


#define vkisvar(k)	(VLOCAL <= (k) && (k) <= VINDEXED)
#define vkisinreg(k)	((k) == VNONRELOC || (k) == VLOCAL)

typedef struct expdesc {
  expkind k;
  union {
    struct {  /* for indexed variables (VINDEXED) */
      short idx;  /* index (R/K) */
      lu_byte t;  /* table (register or upvalue) */
      lu_byte vt;  /* whether 't' is register (VLOCAL) or upvalue (VUPVAL) */
    } ind;
    int info;  /* for generic use */
    lua_Number nval;  /* for VKNUM */
  } u;
  int t;  /* patch list of `exit when true' */
  int f;  /* patch list of `exit when false' */
} expdesc;


/* description of active local variable */
typedef struct Vardesc {
  unsigned short idx;  /* variable index in stack */
} Vardesc;


/* list of all active local variables */
typedef struct Varlist {
  Vardesc *actvar;
  int nactvar;
  int actvarsize;
} Varlist;


/* description of pending goto statement */
typedef struct Gotodesc {
  TString *name;
  int pc;  /* where it is coded */
  int line;  /* line where it appeared */
  lu_byte currlevel;  /* variable level where it appears in current block */
} Gotodesc;


/* list of pending gotos */
typedef struct Gotolist {
  Gotodesc *gt;
  int ngt;
  int gtsize;
} Gotolist;


/* description of active labels */
typedef struct Labeldesc {
  TString *name;
  int pc;  /* label position */
  lu_byte nactvar;  /* variable level where it appears in current block */
} Labeldesc;


/* list of active labels */
typedef struct Labellist {
  Labeldesc *label;
  int nlabel;
  int labelsize;
} Labellist;


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
  int jpc;  /* list of pending jumps to `pc' */
  int freereg;  /* first free register */
  int nk;  /* number of elements in `k' */
  int np;  /* number of elements in `p' */
  int firstlocal;  /* index of first local var of this function */
  short nlocvars;  /* number of elements in `locvars' */
  lu_byte nactvar;  /* number of active local variables */
  lu_byte nups;  /* number of upvalues */
} FuncState;


LUAI_FUNC Proto *luaY_parser (lua_State *L, ZIO *z, Mbuffer *buff,
                              Varlist *varl, Gotolist *gtl,
                              Labellist *labell,  const char *name);


#endif
