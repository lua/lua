/*
** $Id: ldo.c,v 1.84 2000/08/09 19:16:57 roberto Exp roberto $
** Stack and Call structure of Lua
** See Copyright Notice in lua.h
*/


#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"

#include "lauxlib.h"
#include "ldebug.h"
#include "ldo.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lparser.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
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
      lua_error(L, "BAD STACK OVERFLOW! DATA CORRUPTED!");
    }
    else {
      lua_Debug dummy;
      L->stack_last += EXTRA_STACK;  /* to be used by error message */
      if (lua_getstack(L, L->stacksize/SLOTS_PER_F, &dummy) == 0) {
        /* too few funcs on stack: doesn't look like a recursion loop */
        lua_error(L, "Lua2C - C2Lua overflow");
      }
      else
        lua_error(L, "stack overflow; possible recursion loop");
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
      ttype(L->top++) = TAG_NIL;
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


void luaD_lineHook (lua_State *L, StkId func, int line, lua_Hook linehook) {
  if (L->allowhooks) {
    lua_Debug ar;
    struct C_Lua_Stack oldCLS = L->Cstack;
    StkId old_top = L->Cstack.lua2C = L->Cstack.base = L->top;
    L->Cstack.num = 0;
    ar._func = func;
    ar.event = "line";
    ar.currentline = line;
    L->allowhooks = 0;  /* cannot call hooks inside a hook */
    (*linehook)(L, &ar);
    L->allowhooks = 1;
    L->top = old_top;
    L->Cstack = oldCLS;
  }
}


static void luaD_callHook (lua_State *L, StkId func, lua_Hook callhook,
                    const char *event) {
  if (L->allowhooks) {
    lua_Debug ar;
    struct C_Lua_Stack oldCLS = L->Cstack;
    StkId old_top = L->Cstack.lua2C = L->Cstack.base = L->top;
    L->Cstack.num = 0;
    ar._func = func;
    ar.event = event;
    L->allowhooks = 0;  /* cannot call hooks inside a hook */
    callhook(L, &ar);
    L->allowhooks = 1;
    L->top = old_top;
    L->Cstack = oldCLS;
  }
}


static StkId callCclosure (lua_State *L, const struct Closure *cl, StkId base) {
  int nup = cl->nupvalues;  /* number of upvalues */
  int numarg = L->top-base;
  struct C_Lua_Stack oldCLS = L->Cstack;
  StkId firstResult;
  if (nup > 0) {
    int n = numarg;
    luaD_checkstack(L, nup);
    /* open space for upvalues as extra arguments */
    while (n--) *(base+nup+n) = *(base+n);
    L->top += nup;
    numarg += nup;
    /* copy upvalues into stack */
    while (nup--) *(base+nup) = cl->upvalue[nup];
  }
  L->Cstack.num = numarg;
  L->Cstack.lua2C = base;
  L->Cstack.base = L->top;
  (*cl->f.c)(L);  /* do the actual call */
  firstResult = L->Cstack.base;
  L->Cstack = oldCLS;
  return firstResult;
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
  lua_Hook callhook = L->callhook;
  retry:  /* for `function' tag method */
  switch (ttype(func)) {
    case TAG_LCLOSURE: {
      CallInfo ci;
      ci.func = clvalue(func);
      ci.line = 0;
      ttype(func) = TAG_LMARK;
      infovalue(func) = &ci;
      if (callhook)
        luaD_callHook(L, func, callhook, "call");
      firstResult = luaV_execute(L, ci.func, func+1);
      break;
    }
    case TAG_CCLOSURE: {
      ttype(func) = TAG_CMARK;
      if (callhook)
        luaD_callHook(L, func, callhook, "call");
      firstResult = callCclosure(L, clvalue(func), func+1);
      break;
    }
    default: { /* `func' is not a function; check the `function' tag method */
      const TObject *im = luaT_getimbyObj(L, func, IM_FUNCTION);
      if (ttype(im) == TAG_NIL)
        luaG_typeerror(L, func, "call");
      luaD_openstack(L, func);
      *func = *im;  /* tag method is the new function to be called */
      goto retry;   /* retry the call */
    }
  }
  if (callhook)  /* same hook that was active at entry */
    luaD_callHook(L, func, callhook, "return");
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
  const TObject *em = luaH_getglobal(L, LUA_ERRORMESSAGE);
  if (*luaO_typename(em) == 'f') {
    *L->top = *em;
    incr_top;
    lua_pushstring(L, s);
    luaD_call(L, L->top-2, 0);
  }
}


void luaD_breakrun (lua_State *L, int errcode) {
  if (L->errorJmp) {
    L->errorJmp->status = errcode;
    longjmp(L->errorJmp->b, 1);
  }
  else {
    if (errcode != LUA_ERRMEM)
      message(L, "unable to recover; exiting\n");
    exit(1);
  }
}

/*
** Reports an error, and jumps up to the available recovery label
*/
void lua_error (lua_State *L, const char *s) {
  if (s) message(L, s);
  luaD_breakrun(L, LUA_ERRRUN);
}


static void chain_longjmp (lua_State *L, struct lua_longjmp *lj) {
  lj->base = L->Cstack.base;
  lj->numCblocks = L->numCblocks;
  lj->previous = L->errorJmp;
  L->errorJmp = lj;
}


static void restore_longjmp (lua_State *L, struct lua_longjmp *lj) {
  L->Cstack.num = 0;  /* no results */
  L->top = L->Cstack.base = L->Cstack.lua2C = lj->base;
  L->numCblocks = lj->numCblocks;
  L->errorJmp = lj->previous;
}


/*
** Execute a protected call. Assumes that function is at Cstack.base and
** parameters are on top of it.
*/
int luaD_protectedrun (lua_State *L) {
  struct lua_longjmp myErrorJmp;
  chain_longjmp(L, &myErrorJmp);
  if (setjmp(myErrorJmp.b) == 0) {
    StkId base = L->Cstack.base;
    luaD_call(L, base, MULT_RET);
    L->Cstack.lua2C = base;  /* position of the new results */
    L->Cstack.num = L->top - base;
    L->Cstack.base = base + L->Cstack.num;  /* incorporate results on stack */
    L->errorJmp = myErrorJmp.previous;
    return 0;
  }
  else {  /* an error occurred: restore the stack */
    restore_longjmp(L, &myErrorJmp);
    restore_stack_limit(L);
    return myErrorJmp.status;
  }
}


/*
** returns 0 = chunk loaded; >0 : error; -1 = no more chunks to load
*/
static int protectedparser (lua_State *L, ZIO *z, int bin) {
  struct lua_longjmp myErrorJmp;
  chain_longjmp(L, &myErrorJmp);
  L->top = L->Cstack.base;   /* clear C2Lua */
  if (setjmp(myErrorJmp.b) == 0) {
    Proto *tf = bin ? luaU_undump1(L, z) : luaY_parser(L, z);
    L->errorJmp = myErrorJmp.previous;
    if (tf == NULL) return -1;  /* `natural' end */
    luaV_Lclosure(L, tf, 0);
    return 0;
  }
  else {  /* an error occurred */
    restore_longjmp(L, &myErrorJmp);
    if (myErrorJmp.status == LUA_ERRRUN)
      myErrorJmp.status = LUA_ERRSYNTAX;
    return myErrorJmp.status;  /* error code */
  }
}


static int do_main (lua_State *L, ZIO *z, int bin) {
  int status;
  do {
    unsigned long old_blocks;
    luaC_checkGC(L);
    old_blocks = L->nblocks;
    status = protectedparser(L, z, bin);
    if (status > 0) return status;  /* error */
    else if (status < 0) return 0;  /* `natural' end */
    else {
      unsigned long newelems2 = 2*(L->nblocks-old_blocks);
      L->GCthreshold += newelems2;
      status = luaD_protectedrun(L);
      L->GCthreshold -= newelems2;
    }
  } while (bin && status == 0);
  return status;
}


int lua_dofile (lua_State *L, const char *filename) {
  ZIO z;
  int status;
  int bin;  /* flag for file mode */
  int c;    /* look ahead char */
  char source[MAXFILENAME];
  FILE *f = (filename == NULL) ? stdin : fopen(filename, "r");
  if (f == NULL) return 2;  /* unable to open file */
  luaL_filesource(source, filename, sizeof(source));
  c = fgetc(f);
  ungetc(c, f);
  bin = (c == ID_CHUNK);
  if (bin && f != stdin) {
    f = freopen(filename, "rb", f);  /* set binary mode */
    if (f == NULL) return 2;  /* unable to reopen file */
  }
  luaZ_Fopen(&z, f, source);
  status = do_main(L, &z, bin);
  if (f != stdin)
    fclose(f);
  return status;
}


int lua_dostring (lua_State *L, const char *str) {
  return lua_dobuffer(L, str, strlen(str), str);
}


int lua_dobuffer (lua_State *L, const char *buff, size_t size,
                  const char *name) {
  ZIO z;
  if (!name) name = "?";
  luaZ_mopen(&z, buff, size, name);
  return do_main(L, &z, buff[0]==ID_CHUNK);
}

