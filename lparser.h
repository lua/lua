/*
** $Id: lparser.h,v 1.12 2000/03/10 18:37:44 roberto Exp roberto $
** LL(1) Parser and code generator for Lua
** See Copyright Notice in lua.h
*/

#ifndef lparser_h
#define lparser_h

#include "lobject.h"
#include "lzio.h"


/* maximum number of local variables */
#ifndef MAXLOCALS
#define MAXLOCALS 200		/* arbitrary limit (<=MAXARG_B) */
#endif


/* maximum number of upvalues */
#ifndef MAXUPVALUES
#define MAXUPVALUES 32		/* arbitrary limit (<=MAXARG_B) */
#endif


/* maximum number of variables in the left side of an assignment */
#ifndef MAXVARSLH
#define MAXVARSLH 100		/* arbitrary limit (<=MAXARG_B) */
#endif


/* maximum number of parameters in a function */
#ifndef MAXPARAMS
#define MAXPARAMS 100		/* arbitrary limit (<=MAXLOCALS) */
#endif


/* maximum stack size in a function */
#ifndef MAXSTACK
#define MAXSTACK 256		/* arbitrary limit (<=MAXARG_A) */
#endif



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


#define NO_JUMP	(-1)		/* marks end of patch list */


/* state needed to generate code for a given function */
typedef struct FuncState {
  Proto *f;  /* current function header */
  struct FuncState *prev;  /* enclosing function */
  struct LexState *ls;  /* lexical state */
  struct lua_State *L;  /* copy of the Lua state */
  int pc;  /* next position to code */
  int lasttarget;   /* `pc' of last `jump target' */
  int stacksize;  /* number of values on activation register */
  int nlocalvar;  /* number of active local variables */
  int nupvalues;  /* number of upvalues */
  int nvars;  /* number of entries in f->locvars (-1 if no debug information) */
  int lastsetline;  /* line where last SETLINE was issued */
  expdesc upvalues[MAXUPVALUES];  /* upvalues */
  TString *localvar[MAXLOCALS];  /* store local variable names */
} FuncState;


Proto *luaY_parser (lua_State *L, ZIO *z);


#endif
