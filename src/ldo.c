/*
** $Id: ldo.c,v 2.7 2004/06/02 19:07:55 roberto Exp $
** Stack and Call structure of Lua
** See Copyright Notice in lua.h
*/


#include <stdlib.h>
#include <string.h>

#define ldo_c
#define LUA_CORE

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lparser.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lundump.h"
#include "lvm.h"
#include "lzio.h"




/*
** {======================================================
** Error-recovery functions
** =======================================================
*/


/* chain list of long jump buffers */
struct lua_longjmp {
  struct lua_longjmp *previous;
  l_jmpbuf b;
  volatile int status;  /* error code */
};


static void seterrorobj (lua_State *L, int errcode, StkId oldtop) {
  switch (errcode) {
    case LUA_ERRMEM: {
      setsvalue2s(L, oldtop, luaS_newliteral(L, MEMERRMSG));
      break;
    }
    case LUA_ERRERR: {
      setsvalue2s(L, oldtop, luaS_newliteral(L, "error in error handling"));
      break;
    }
    case LUA_ERRSYNTAX:
    case LUA_ERRRUN: {
      setobjs2s(L, oldtop, L->top - 1);  /* error message on current top */
      break;
    }
  }
  L->top = oldtop + 1;
}


void luaD_throw (lua_State *L, int errcode) {
  if (L->errorJmp) {
    L->errorJmp->status = errcode;
    L_THROW(L->errorJmp);
  }
  else {
    if (G(L)->panic) G(L)->panic(L);
    exit(EXIT_FAILURE);
  }
}


int luaD_rawrunprotected (lua_State *L, Pfunc f, void *ud) {
  struct lua_longjmp lj;
  lj.status = 0;
  lj.previous = L->errorJmp;  /* chain new error handler */
  L->errorJmp = &lj;
  L_TRY(&lj,
    (*f)(L, ud);
  );
  L->errorJmp = lj.previous;  /* restore old error handler */
  return lj.status;
}


static void restore_stack_limit (lua_State *L) {
  L->stack_last = L->stack+L->stacksize-1;
  if (L->size_ci > LUA_MAXCALLS) {  /* there was an overflow? */
    int inuse = (L->ci - L->base_ci);
    if (inuse + 1 < LUA_MAXCALLS)  /* can `undo' overflow? */
      luaD_reallocCI(L, LUA_MAXCALLS);
  }
}

/* }====================================================== */


static void correctstack (lua_State *L, TValue *oldstack) {
  CallInfo *ci;
  GCObject *up;
  L->top = (L->top - oldstack) + L->stack;
  for (up = L->openupval; up != NULL; up = up->gch.next)
    gco2uv(up)->v = (gco2uv(up)->v - oldstack) + L->stack;
  for (ci = L->base_ci; ci <= L->ci; ci++) {
    ci->top = (ci->top - oldstack) + L->stack;
    ci->base = (ci->base - oldstack) + L->stack;
    ci->func = (ci->func - oldstack) + L->stack;
  }
  L->base = L->ci->base;
}


void luaD_reallocstack (lua_State *L, int newsize) {
  TValue *oldstack = L->stack;
  luaM_reallocvector(L, L->stack, L->stacksize, newsize, TValue);
  L->stacksize = newsize;
  L->stack_last = L->stack+newsize-1-EXTRA_STACK;
  correctstack(L, oldstack);
}


void luaD_reallocCI (lua_State *L, int newsize) {
  CallInfo *oldci = L->base_ci;
  luaM_reallocvector(L, L->base_ci, L->size_ci, newsize, CallInfo);
  L->size_ci = cast(unsigned short, newsize);
  L->ci = (L->ci - oldci) + L->base_ci;
  L->end_ci = L->base_ci + L->size_ci;
}


void luaD_growstack (lua_State *L, int n) {
  if (n <= L->stacksize)  /* double size is enough? */
    luaD_reallocstack(L, 2*L->stacksize);
  else
    luaD_reallocstack(L, L->stacksize + n + EXTRA_STACK);
}


static void luaD_growCI (lua_State *L) {
  if (L->size_ci > LUA_MAXCALLS)  /* overflow while handling overflow? */
    luaD_throw(L, LUA_ERRERR);
  else {
    luaD_reallocCI(L, 2*L->size_ci);
    if (L->size_ci > LUA_MAXCALLS)
      luaG_runerror(L, "stack overflow");
  }
}


void luaD_callhook (lua_State *L, int event, int line) {
  lua_Hook hook = L->hook;
  if (hook && L->allowhook) {
    ptrdiff_t top = savestack(L, L->top);
    ptrdiff_t ci_top = savestack(L, L->ci->top);
    lua_Debug ar;
    ar.event = event;
    ar.currentline = line;
    if (event == LUA_HOOKTAILRET)
      ar.i_ci = 0;  /* tail call; no debug information about it */
    else
      ar.i_ci = L->ci - L->base_ci;
    luaD_checkstack(L, LUA_MINSTACK);  /* ensure minimum stack size */
    L->ci->top = L->top + LUA_MINSTACK;
    L->allowhook = 0;  /* cannot call hooks inside a hook */
    lua_unlock(L);
    (*hook)(L, &ar);
    lua_lock(L);
    lua_assert(!L->allowhook);
    L->allowhook = 1;
    L->ci->top = restorestack(L, ci_top);
    L->top = restorestack(L, top);
  }
}


static StkId adjust_varargs (lua_State *L, int nfixargs, int actual,
                             int style) {
  int i;
  Table *htab = NULL;
  StkId base, fixed;
  if (actual < nfixargs) {
    for (; actual < nfixargs; ++actual)
      setnilvalue(L->top++);
  }
  if (style != NEWSTYLEVARARG) {  /* compatibility with old-style vararg */
    int nvar = actual - nfixargs;  /* number of extra arguments */
    htab = luaH_new(L, nvar, 1);  /* create `arg' table */
    for (i=0; i<nvar; i++)  /* put extra arguments into `arg' table */
      setobj2n(L, luaH_setnum(L, htab, i+LUA_FIRSTINDEX), L->top - nvar + i);
    /* store counter in field `n' */
    setnvalue(luaH_setstr(L, htab, luaS_newliteral(L, "n")),
                                   cast(lua_Number, nvar));
  }
  /* move fixed parameters to final position */
  fixed = L->top - actual;  /* first fixed argument */
  base = L->top;  /* final position of first argument */
  for (i=0; i<nfixargs; i++) {
    setobjs2s(L, L->top++, fixed+i);
    setnilvalue(fixed+i);
  }
  /* add `arg' parameter */
  if (htab) {
    sethvalue(L, L->top++, htab);
    lua_assert(iswhite(obj2gco(htab)));
  }
  return base;
}


static StkId tryfuncTM (lua_State *L, StkId func) {
  const TValue *tm = luaT_gettmbyobj(L, func, TM_CALL);
  StkId p;
  ptrdiff_t funcr = savestack(L, func);
  if (!ttisfunction(tm))
    luaG_typeerror(L, func, "call");
  /* Open a hole inside the stack at `func' */
  for (p = L->top; p > func; p--) setobjs2s(L, p, p-1);
  incr_top(L);
  func = restorestack(L, funcr);  /* previous call may change stack */
  setobj2s(L, func, tm);  /* tag method is the new function to be called */
  return func;
}


int luaD_precall (lua_State *L, StkId func, int nresults) {
  LClosure *cl;
  ptrdiff_t funcr;
  if (!ttisfunction(func)) /* `func' is not a function? */
    func = tryfuncTM(L, func);  /* check the `function' tag method */
  funcr = savestack(L, func);
  if (L->ci + 1 == L->end_ci) luaD_growCI(L);
  else condhardstacktests(luaD_reallocCI(L, L->size_ci));
  cl = &clvalue(func)->l;
  if (!cl->isC) {  /* Lua function? prepare its call */
    CallInfo *ci;
    StkId st, base;
    Proto *p = cl->p;
    luaD_checkstack(L, p->maxstacksize);
    func = restorestack(L, funcr);
    if (p->is_vararg) {  /* varargs? */
      int nargs = L->top - func - 1;
      base = adjust_varargs(L, p->numparams, nargs, p->is_vararg);
    }
    else
      base = func + 1;
    ci = ++L->ci;  /* now `enter' new function */
    ci->func = func;
    L->base = ci->base = base;
    ci->top = L->base + p->maxstacksize;
    ci->savedpc = p->code;  /* starting point */
    ci->tailcalls = 0;
    ci->nresults = nresults;
    for (st = L->top; st < ci->top; st++)
      setnilvalue(st);
    L->top = ci->top;
    return PCRLUA;
  }
  else {  /* if is a C function, call it */
    CallInfo *ci;
    int n;
    luaD_checkstack(L, LUA_MINSTACK);  /* ensure minimum stack size */
    ci = ++L->ci;  /* now `enter' new function */
    ci->func = restorestack(L, funcr);
    L->base = ci->base = ci->func + 1;
    ci->top = L->top + LUA_MINSTACK;
    if (L->hookmask & LUA_MASKCALL)
      luaD_callhook(L, LUA_HOOKCALL, -1);
    lua_unlock(L);
    n = (*curr_func(L)->c.f)(L);  /* do the actual call */
    lua_lock(L);
    if (n >= 0) {  /* no yielding? */
      luaD_poscall(L, nresults, L->top - n);
      return PCRC;
    }
    else {
      ci->nresults = nresults;
      return PCRYIELD;
    }
  }
}


static StkId callrethooks (lua_State *L, StkId firstResult) {
  ptrdiff_t fr = savestack(L, firstResult);  /* next call may change stack */
  luaD_callhook(L, LUA_HOOKRET, -1);
  if (f_isLua(L->ci)) {  /* Lua function? */
    while (L->ci->tailcalls--)  /* call hook for eventual tail calls */
      luaD_callhook(L, LUA_HOOKTAILRET, -1);
  }
  return restorestack(L, fr);
}


void luaD_poscall (lua_State *L, int wanted, StkId firstResult) { 
  StkId res;
  if (L->hookmask & LUA_MASKRET)
    firstResult = callrethooks(L, firstResult);
  res = L->ci->func;  /* res == final position of 1st result */
  L->ci--;
  L->base = L->ci->base;  /* restore base */
  /* move results to correct place */
  while (wanted != 0 && firstResult < L->top) {
    setobjs2s(L, res++, firstResult++);
    wanted--;
  }
  while (wanted-- > 0)
    setnilvalue(res++);
  L->top = res;
}


/*
** Call a function (C or Lua). The function to be called is at *func.
** The arguments are on the stack, right after the function.
** When returns, all the results are on the stack, starting at the original
** function position.
*/ 
void luaD_call (lua_State *L, StkId func, int nResults) {
  if (++L->nCcalls >= LUA_MAXCCALLS) {
    if (L->nCcalls == LUA_MAXCCALLS)
      luaG_runerror(L, "C stack overflow");
    else if (L->nCcalls >= (LUA_MAXCCALLS + (LUA_MAXCCALLS>>3)))
      luaD_throw(L, LUA_ERRERR);  /* error while handing stack error */
  }
  if (luaD_precall(L, func, nResults) == PCRLUA) {  /* is a Lua function? */
    StkId firstResult = luaV_execute(L, 1);  /* call it */
    luaD_poscall(L, nResults, firstResult);
  }
  L->nCcalls--;
  luaC_checkGC(L);
}


static void resume (lua_State *L, void *ud) {
  StkId firstResult;
  int nargs = *cast(int *, ud);
  CallInfo *ci = L->ci;
  if (!L->isSuspended) {
    lua_assert(ci == L->base_ci && nargs < L->top - L->base);
    luaD_precall(L, L->top - (nargs + 1), LUA_MULTRET);  /* start coroutine */
  }
  else {  /* resuming from previous yield */
    if (!f_isLua(ci)) {  /* `common' yield? */
      /* finish interrupted execution of `OP_CALL' */
      int nresults = ci->nresults;
      lua_assert(GET_OPCODE(*((ci-1)->savedpc - 1)) == OP_CALL ||
                 GET_OPCODE(*((ci-1)->savedpc - 1)) == OP_TAILCALL);
      luaD_poscall(L, nresults, L->top - nargs);  /* complete it */
      if (nresults >= 0) L->top = L->ci->top;
    }  /* else yielded inside a hook: just continue its execution */
  }
  L->isSuspended = 0;
  firstResult = luaV_execute(L, L->ci - L->base_ci);
  if (firstResult != NULL)   /* return? */
    luaD_poscall(L, LUA_MULTRET, firstResult);  /* finalize this coroutine */
}


static int resume_error (lua_State *L, const char *msg) {
  L->top = L->ci->base;
  setsvalue2s(L, L->top, luaS_new(L, msg));
  incr_top(L);
  lua_unlock(L);
  return LUA_ERRRUN;
}


LUA_API int lua_resume (lua_State *L, int nargs) {
  int status;
  lu_byte old_allowhooks;
  lua_lock(L);
  lua_assert(L->errfunc == 0 && L->nCcalls == 0);
  if (!L->isSuspended) {
    if (L->ci == L->base_ci) {  /* no activation record? */
      if (nargs >= L->top - L->base)
        return resume_error(L, "cannot resume dead coroutine");
    }
    else
      return resume_error(L, "cannot resume non-suspended coroutine");
  }
  old_allowhooks = L->allowhook;
  status = luaD_rawrunprotected(L, resume, &nargs);
  if (status != 0) {  /* error? */
    L->ci = L->base_ci;  /* go back to initial level */
    L->base = L->ci->base;
    L->nCcalls = 0;
    luaF_close(L, L->base);  /* close eventual pending closures */
    seterrorobj(L, status, L->base);
    L->allowhook = old_allowhooks;
    restore_stack_limit(L);
  }
  lua_unlock(L);
  return status;
}


LUA_API int lua_yield (lua_State *L, int nresults) {
  CallInfo *ci;
  lua_lock(L);
  ci = L->ci;
  if (L->nCcalls > 0)
    luaG_runerror(L, "attempt to yield across metamethod/C-call boundary");
  if (!f_isLua(ci)) {  /* usual yield */
    if (L->top - nresults > L->base) {  /* is there garbage in the stack? */
      int i;
      for (i=0; i<nresults; i++)  /* move down results */
        setobjs2s(L, L->base + i, L->top - nresults + i);
      L->top = L->base + nresults;
    }
  } /* else it's an yield inside a hook: nothing to do */
  L->isSuspended = 1;
  lua_unlock(L);
  return -1;
}


int luaD_pcall (lua_State *L, Pfunc func, void *u,
                ptrdiff_t old_top, ptrdiff_t ef) {
  int status;
  unsigned short oldnCcalls = L->nCcalls;
  ptrdiff_t old_ci = saveci(L, L->ci);
  lu_byte old_allowhooks = L->allowhook;
  ptrdiff_t old_errfunc = L->errfunc;
  L->errfunc = ef;
  status = luaD_rawrunprotected(L, func, u);
  if (status != 0) {  /* an error occurred? */
    StkId oldtop = restorestack(L, old_top);
    luaF_close(L, oldtop);  /* close eventual pending closures */
    seterrorobj(L, status, oldtop);
    L->nCcalls = oldnCcalls;
    L->ci = restoreci(L, old_ci);
    L->base = L->ci->base;
    L->allowhook = old_allowhooks;
    restore_stack_limit(L);
  }
  L->errfunc = old_errfunc;
  return status;
}



/*
** Execute a protected parser.
*/
struct SParser {  /* data to `f_parser' */
  ZIO *z;
  Mbuffer buff;  /* buffer to be used by the scanner */
  const char *name;
};

static void f_parser (lua_State *L, void *ud) {
  int i;
  Proto *tf;
  Closure *cl;
  struct SParser *p = cast(struct SParser *, ud);
  int c = luaZ_lookahead(p->z);
  luaC_checkGC(L);
  tf = ((c == LUA_SIGNATURE[0]) ? luaU_undump : luaY_parser)(L, p->z,
                                                             &p->buff, p->name);
  cl = luaF_newLclosure(L, tf->nups, gt(L));
  cl->l.p = tf;
  for (i = 0; i < tf->nups; i++)  /* initialize eventual upvalues */
    cl->l.upvals[i] = luaF_newupval(L);
  setclvalue(L, L->top, cl);
  incr_top(L);
}


int luaD_protectedparser (lua_State *L, ZIO *z, const char *name) {
  struct SParser p;
  int status;
  p.z = z; p.name = name;
  luaZ_initbuffer(L, &p.buff);
  status = luaD_pcall(L, f_parser, &p, savestack(L, L->top), L->errfunc);
  luaZ_freebuffer(L, &p.buff);
  return status;
}


