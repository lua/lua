/*
** $Id: lparser.h,v 1.18 2000/06/21 18:13:56 roberto Exp roberto $
** LL(1) Parser and code generator for Lua
** See Copyright Notice in lua.h
*/

#ifndef lparser_h
#define lparser_h

#include "lobject.h"
#include "lzio.h"


/*
** Expression descriptor
*/

typedef enum {
  VGLOBAL,
  VLOCAL,
  VINDEXED,
  VEXP
} expkind;

typedef struct expdesc {
  expkind k;
  union {
    int index;  /* VGLOBAL: `kstr' index of global name; VLOCAL: stack index */
    struct {
      int t;  /* patch list of `exit when true' */
      int f;  /* patch list of `exit when false' */
    } l;
  } u;
} expdesc;



/* state needed to generate code for a given function */
typedef struct FuncState {
  Proto *f;  /* current function header */
  struct FuncState *prev;  /* enclosing function */
  struct LexState *ls;  /* lexical state */
  struct lua_State *L;  /* copy of the Lua state */
  int pc;  /* next position to code */
  int lasttarget;   /* `pc' of last `jump target' */
  int jlt;  /* list of jumps to `lasttarged' */
  int stacklevel;  /* number of values on activation register */
  int nlocalvar;  /* number of active local variables */
  int nupvalues;  /* number of upvalues */
  int nvars;  /* number of entries in f->locvars */
  struct Breaklabel *bl;  /* chain of breakable blocks */
  expdesc upvalues[MAXUPVALUES];  /* upvalues */
  TString *localvar[MAXLOCALS];  /* store local variable names */
} FuncState;


Proto *luaY_parser (lua_State *L, ZIO *z);


#endif
