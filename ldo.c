/*
** $Id: ldo.c,v 1.59 1999/12/21 18:04:41 roberto Exp roberto $
** Stack and Call structure of Lua
** See Copyright Notice in lua.h
*/


#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LUA_REENTRANT

#include "lauxlib.h"
#include "ldo.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lparser.h"
#include "lstate.h"
#include "lstring.h"
#include "ltm.h"
#include "lua.h"
#include "luadebug.h"
#include "lundump.h"
#include "lvm.h"
#include "lzio.h"


#define EXTRA_STACK	32	/* space to handle stack overflow errors */

/*
** typical numer of stack slots used by a (big) function
** (this constant is used only for choosing error messages)
*/
#define SLOTS_PER_F	20


void luaD_init (lua_State *L, int stacksize) {
  L->stack = luaM_newvector(L, stacksize+EXTRA_STACK, TObject);
  L->stack_last = L->stack+(stacksize-1);
  L->stacksize = stacksize;
  L->Cstack.base = L->Cstack.lua2C = L->top = L->stack;
  L->Cstack.num = 0;
}


void luaD_checkstack (lua_State *L, int n) {
  if (L->stack_last-L->top <= n) {  /* stack overflow? */
    if (L->stack_last-L->stack > (L->stacksize-1)) {
      /* overflow while handling overflow: do what?? */
      L->top -= EXTRA_STACK;
      lua_error(L, "BAD STACK OVERFLOW! DATA CORRUPTED!!");
    }
    else {
      L->stack_last += EXTRA_STACK;  /* to be used by error message */
      if (lua_stackedfunction(L, L->stacksize/SLOTS_PER_F) == LUA_NOOBJECT) {
        /* too few funcs on stack: doesn't look like a rec. loop */
        lua_error(L, "Lua2C - C2Lua overflow");
      }
      else
        lua_error(L, "stack size overflow");
    }
  }
}


static void restore_stack_limit (lua_State *L) {
  if (L->top-L->stack < L->stacksize-1)
    L->stack_last = L->stack+(L->stacksize-1);
}


/*
** Adjust stack. Set top to base+extra, pushing NILs if needed.
** (we cannot add base+extra unless we are sure it fits in the stack;
**  otherwise the result of such operation on pointers is undefined)
*/
void luaD_adjusttop (lua_State *L, StkId base, int extra) {
  int diff = extra-(L->top-base);
  if (diff <= 0)
    L->top = base+extra;
  else {
    luaD_checkstack(L, diff);
    while (diff--)
      ttype(L->top++) = LUA_T_NIL;
  }
}


/*
** Open a hole inside the stack at `pos'
*/
void luaD_openstack (lua_State *L, StkId pos) {
  int i = L->top-pos; 
  while (i--) pos[i+1] = pos[i];
  incr_top;
}


void luaD_lineHook (lua_State *L, int line) {
  if (L->allowhooks) {
    struct C_Lua_Stack oldCLS = L->Cstack;
    StkId old_top = L->Cstack.lua2C = L->Cstack.base = L->top;
    L->Cstack.num = 0;
    L->allowhooks = 0;  /* cannot call hooks inside a hook */
    (*L->linehook)(L, line);
    L->allowhooks = 1;
    L->top = old_top;
    L->Cstack = oldCLS;
  }
}


static void luaD_callHook (lua_State *L, StkId func, lua_CHFunction callhook,
                           int isreturn) {
  if (L->allowhooks) {
    struct C_Lua_Stack oldCLS = L->Cstack;
    StkId old_top = L->Cstack.lua2C = L->Cstack.base = L->top;
    L->Cstack.num = 0;
    L->allowhooks = 0;  /* cannot call hooks inside a hook */
    if (isreturn)
      callhook(L, LUA_NOOBJECT, "(return)", 0);
    else {
      switch (ttype(func)) {
        case LUA_T_LPROTO:
          callhook(L, func, tfvalue(func)->source->str,
                            tfvalue(func)->lineDefined);
          break;
        case LUA_T_LCLOSURE:
          callhook(L, func, tfvalue(protovalue(func))->source->str,
                            tfvalue(protovalue(func))->lineDefined);
          break;
        default:
          callhook(L, func, "(C)", -1);
      }
    }
    L->allowhooks = 1;
    L->top = old_top;
    L->Cstack = oldCLS;
  }
}


/*
** Call a C function.
** Cstack.num is the number of arguments; Cstack.lua2C points to the
** first argument. Returns an index to the first result from C.
*/
static StkId callC (lua_State *L, lua_CFunction f, StkId base) {
  struct C_Lua_Stack oldCLS = L->Cstack;
  StkId firstResult;
  int numarg = L->top - base;
  L->Cstack.num = numarg;
  L->Cstack.lua2C = base;
  L->Cstack.base = L->top;
  (*f)(L);  /* do the actual call */
  firstResult = L->Cstack.base;
  L->Cstack = oldCLS;
  return firstResult;
}


static StkId callCclosure (lua_State *L, const struct Closure *cl, StkId base) {
  int nup = cl->nelems;  /* number of upvalues */
  int n = L->top-base;   /* number of arguments (to move up) */
  luaD_checkstack(L, nup);
  /* open space for upvalues as extra arguments */
  while (n--) *(base+nup+n) = *(base+n);
  L->top += nup;
  /* copy upvalues into stack */
  while (nup--) *(base+nup) = cl->consts[nup+1];
  return callC(L, fvalue(cl->consts), base);
}


void luaD_callTM (lua_State *L, const TObject *f, int nParams, int nResults) {
  StkId base = L->top - nParams;
  luaD_openstack(L, base);
  *base = *f;
  luaD_call(L, base, nResults);
}


/*
** Call a function (C or Lua). The function to be called is at *func.
** The arguments are on the stack, right after the function.
** When returns, the results are on the stack, starting at the original
** function position.
** The number of results is nResults, unless nResults=MULT_RET.
*/ 
void luaD_call (lua_State *L, StkId func, int nResults) {
  StkId firstResult;
  lua_CHFunction callhook = L->callhook;
  if (callhook)
    luaD_callHook(L, func, callhook, 0);
  retry:  /* for `function' tag method */
  switch (ttype(func)) {
    case LUA_T_CPROTO:
      ttype(func) = LUA_T_CMARK;
      firstResult = callC(L, fvalue(func), func+1);
      break;
    case LUA_T_LPROTO:
      ttype(func) = LUA_T_LMARK;
      firstResult = luaV_execute(L, NULL, tfvalue(func), func+1);
      break;
    case LUA_T_LCLOSURE: {
      Closure *c = clvalue(func);
      ttype(func) = LUA_T_LCLMARK;
      firstResult = luaV_execute(L, c, tfvalue(c->consts), func+1);
      break;
    }
    case LUA_T_CCLOSURE: {
      Closure *c = clvalue(func);
      ttype(func) = LUA_T_CCLMARK;
      firstResult = callCclosure(L, c, func+1);
      break;
    }
    default: { /* `func' is not a function; check the `function' tag method */
      const TObject *im = luaT_getimbyObj(L, func, IM_FUNCTION);
      if (ttype(im) == LUA_T_NIL)
        lua_error(L, "call expression not a function");
      luaD_openstack(L, func);
      *func = *im;  /* tag method is the new function to be called */
      goto retry;  /* retry the call (without calling callhook again) */
    }
  }
  if (callhook)  /* same hook that was used at entry */
    luaD_callHook(L, NULL, callhook, 1);  /* `return' hook */
  /* adjust the number of results */
  if (nResults == MULT_RET)
    nResults = L->top - firstResult;
  else
    luaD_adjusttop(L, firstResult, nResults);
  /* move results to `func' (to erase parameters and function) */
  while (nResults) {
    *func++ = *(L->top - nResults);
    nResults--;
  }
  L->top = func;
}


static void message (lua_State *L, const char *s) {
  const TObject *em = &(luaS_assertglobalbyname(L, "_ERRORMESSAGE")->value);
  if (*luaO_typename(em) == 'f') {
    *L->top = *em;
    incr_top;
    lua_pushstring(L, s);
    luaD_call(L, L->top-2, 0);
  }
}

/*
** Reports an error, and jumps up to the available recover label
*/
void lua_error (lua_State *L, const char *s) {
  if (s) message(L, s);
  if (L->errorJmp)
    longjmp(L->errorJmp->b, 1);
  else {
    message(L, "exit(1). Unable to recover.\n");
    exit(1);
  }
}


/*
** Execute a protected call. Assumes that function is at Cstack.base and
** parameters are on top of it.
*/
int luaD_protectedrun (lua_State *L) {
  struct lua_longjmp myErrorJmp;
  volatile StkId base = L->Cstack.base;
  volatile int numCblocks = L->numCblocks;
  volatile int status;
  struct lua_longjmp *volatile oldErr = L->errorJmp;
  L->errorJmp = &myErrorJmp;
  if (setjmp(myErrorJmp.b) == 0) {
    luaD_call(L, base, MULT_RET);
    L->Cstack.lua2C = base;  /* position of the new results */
    L->Cstack.num = L->top - base;
    L->Cstack.base = base + L->Cstack.num;  /* incorporate results on stack */
    status = 0;
  }
  else {  /* an error occurred: restore the stack */
    L->Cstack.num = 0;  /* no results */
    L->top = L->Cstack.base = L->Cstack.lua2C = base;
    L->numCblocks = numCblocks;
    restore_stack_limit(L);
    status = 1;
  }
  L->errorJmp = oldErr;
  return status;
}


/*
** returns 0 = chunk loaded; 1 = error; 2 = no more chunks to load
*/
static int protectedparser (lua_State *L, ZIO *z, int bin) {
  struct lua_longjmp myErrorJmp;
  volatile StkId base = L->Cstack.base;
  volatile int numCblocks = L->numCblocks;
  volatile int status;
  TProtoFunc *volatile tf;
  struct lua_longjmp *volatile oldErr = L->errorJmp;
  L->errorJmp = &myErrorJmp;
  L->top = base;   /* erase C2Lua */
  if (setjmp(myErrorJmp.b) == 0) {
    tf = bin ? luaU_undump1(L, z) : luaY_parser(L, z);
    status = 0;
  }
  else {  /* an error occurred: restore Cstack and top */
    L->Cstack.num = 0;  /* no results */
    L->top = L->Cstack.base = L->Cstack.lua2C = base;
    L->numCblocks = numCblocks;
    tf = NULL;
    status = 1;
  }
  L->errorJmp = oldErr;
  if (status) return 1;  /* error code */
  if (tf == NULL) return 2;  /* `natural' end */
  L->top->ttype = LUA_T_LPROTO;  /* push new function on the stack */
  L->top->value.tf = tf;
  incr_top;
  return 0;
}


static int do_main (lua_State *L, ZIO *z, int bin) {
  int status;
  int debug = L->debug;  /* save debug status */
  do {
    long old_blocks = (luaC_checkGC(L), L->nblocks);
    status = protectedparser(L, z, bin);
    if (status == 1) return 1;  /* error */
    else if (status == 2) return 0;  /* `natural' end */
    else {
      unsigned long newelems2 = 2*(L->nblocks-old_blocks);
      L->GCthreshold += newelems2;
      status = luaD_protectedrun(L);
      L->GCthreshold -= newelems2;
    }
  } while (bin && status == 0);
  L->debug = debug;  /* restore debug status */
  return status;
}


void luaD_gcIM (lua_State *L, const TObject *o) {
  const TObject *im = luaT_getimbyObj(L, o, IM_GC);
  if (ttype(im) != LUA_T_NIL) {
    *L->top = *o;
    incr_top;
    luaD_callTM(L, im, 1, 0);
  }
}


#define	MAXFILENAME	260	/* maximum part of a file name kept */

int lua_dofile (lua_State *L, const char *filename) {
  ZIO z;
  int status;
  int c;
  int bin;
  char source[MAXFILENAME];
  FILE *f = (filename == NULL) ? stdin : fopen(filename, "r");
  if (f == NULL)
    return 2;
  c = fgetc(f);
  ungetc(c, f);
  bin = (c == ID_CHUNK);
  if (bin)
    f = freopen(filename, "rb", f);  /* set binary mode */
  luaL_filesource(source, filename, sizeof(source));
  luaZ_Fopen(&z, f, source);
  status = do_main(L, &z, bin);
  if (f != stdin)
    fclose(f);
  return status;
}


int lua_dostring (lua_State *L, const char *str) {
  return lua_dobuffer(L, str, strlen(str), str);
}


int lua_dobuffer (lua_State *L, const char *buff, int size, const char *name) {
  ZIO z;
  if (!name) name = "?";
  luaZ_mopen(&z, buff, size, name);
  return do_main(L, &z, buff[0]==ID_CHUNK);
}

