/*
** $Id: ldo.c,v 2.68 2009/09/28 16:32:50 roberto Exp roberto $
** Stack and Call structure of Lua
** See Copyright Notice in lua.h
*/


#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#define ldo_c
#define LUA_CORE

#include "lua.h"

#include "lapi.h"
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
  luai_jmpbuf b;
  volatile int status;  /* error code */
};


void luaD_seterrorobj (lua_State *L, int errcode, StkId oldtop) {
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
    case LUA_ERRGCMM:
    case LUA_ERRRUN: {
      setobjs2s(L, oldtop, L->top - 1);  /* error message on current top */
      break;
    }
  }
  L->top = oldtop + 1;
}


void luaD_throw (lua_State *L, int errcode) {
  if (L->errorJmp) {  /* thread has an error handler? */
    L->errorJmp->status = errcode;  /* set status */
    LUAI_THROW(L, L->errorJmp);  /* jump to it */
  }
  else {  /* thread has no error handler */
    L->status = cast_byte(errcode);  /* mark it as dead */
    if (G(L)->mainthread->errorJmp) {  /* main thread has a handler? */
      setobjs2s(L, G(L)->mainthread->top++, L->top - 1);  /* copy error obj. */
      luaD_throw(G(L)->mainthread, errcode);  /* re-throw in main thread */
    }
    else {  /* no handler at all; abort */
      if (G(L)->panic) {  /* panic function? */
        lua_unlock(L);
        G(L)->panic(L);  /* call it (last chance to jump out) */
      }
      abort();
    }
  }
}


int luaD_rawrunprotected (lua_State *L, Pfunc f, void *ud) {
  unsigned short oldnCcalls = G(L)->nCcalls;
  struct lua_longjmp lj;
  lj.status = LUA_OK;
  lj.previous = L->errorJmp;  /* chain new error handler */
  L->errorJmp = &lj;
  LUAI_TRY(L, &lj,
    (*f)(L, ud);
  );
  L->errorJmp = lj.previous;  /* restore old error handler */
  G(L)->nCcalls = oldnCcalls;
  return lj.status;
}

/* }====================================================== */


static void correctstack (lua_State *L, TValue *oldstack) {
  CallInfo *ci;
  GCObject *up;
  L->top = (L->top - oldstack) + L->stack;
  for (up = L->openupval; up != NULL; up = up->gch.next)
    gco2uv(up)->v = (gco2uv(up)->v - oldstack) + L->stack;
  for (ci = L->ci; ci != NULL; ci = ci->previous) {
    ci->top = (ci->top - oldstack) + L->stack;
    ci->func = (ci->func - oldstack) + L->stack;
    if (isLua(ci))
      ci->u.l.base = (ci->u.l.base - oldstack) + L->stack;
  }
}


/* some space for error handling */
#define ERRORSTACKSIZE	(LUAI_MAXSTACK + 200)


void luaD_reallocstack (lua_State *L, int newsize) {
  TValue *oldstack = L->stack;
  int lim = L->stacksize;
  lua_assert(newsize <= LUAI_MAXSTACK || newsize == ERRORSTACKSIZE);
  lua_assert(L->stack_last - L->stack == L->stacksize - EXTRA_STACK);
  luaM_reallocvector(L, L->stack, L->stacksize, newsize, TValue);
  for (; lim < newsize; lim++)
    setnilvalue(L->stack + lim); /* erase new segment */
  L->stacksize = newsize;
  L->stack_last = L->stack + newsize - EXTRA_STACK;
  correctstack(L, oldstack);
}


void luaD_growstack (lua_State *L, int n) {
  int size = L->stacksize;
  if (size > LUAI_MAXSTACK)  /* error after extra size? */
    luaD_throw(L, LUA_ERRERR);
  else {
    int needed = L->top - L->stack + n + EXTRA_STACK;
    int newsize = 2 * size;
    if (newsize > LUAI_MAXSTACK) newsize = LUAI_MAXSTACK;
    if (newsize < needed) newsize = needed;
    if (newsize > LUAI_MAXSTACK) {  /* stack overflow? */
      luaD_reallocstack(L, ERRORSTACKSIZE);
      luaG_runerror(L, "stack overflow");
    }
    else
      luaD_reallocstack(L, newsize);
  }
}


static int stackinuse (lua_State *L) {
  CallInfo *ci;
  StkId lim = L->top;
  for (ci = L->ci; ci != NULL; ci = ci->previous) {
    lua_assert(ci->top <= L->stack_last);
    if (lim < ci->top) lim = ci->top;
  }
  return cast_int(lim - L->stack) + 1;  /* part of stack in use */
}


void luaD_shrinkstack (lua_State *L) {
  int inuse = stackinuse(L);
  int goodsize = inuse + (inuse / 8) + 2*EXTRA_STACK;
  if (goodsize > LUAI_MAXSTACK) goodsize = LUAI_MAXSTACK;
  if (inuse > LUAI_MAXSTACK ||  /* handling stack overflow? */
      goodsize >= L->stacksize)  /* would grow instead of shrink? */
    condmovestack(L);  /* don't change stack (change only for debugging) */
  else
    luaD_reallocstack(L, goodsize);  /* shrink it */
}


void luaD_callhook (lua_State *L, int event, int line) {
  lua_Hook hook = L->hook;
  if (hook && L->allowhook) {
    CallInfo *ci = L->ci;
    ptrdiff_t top = savestack(L, L->top);
    ptrdiff_t ci_top = savestack(L, ci->top);
    lua_Debug ar;
    ar.event = event;
    ar.currentline = line;
    if (event == LUA_HOOKTAILRET)
      ar.i_ci = NULL;  /* tail call; no debug information about it */
    else
      ar.i_ci = ci;
    luaD_checkstack(L, LUA_MINSTACK);  /* ensure minimum stack size */
    ci->top = L->top + LUA_MINSTACK;
    lua_assert(ci->top <= L->stack_last);
    L->allowhook = 0;  /* cannot call hooks inside a hook */
    ci->callstatus |= CIST_HOOKED;
    lua_unlock(L);
    (*hook)(L, &ar);
    lua_lock(L);
    lua_assert(!L->allowhook);
    L->allowhook = 1;
    ci->top = restorestack(L, ci_top);
    L->top = restorestack(L, top);
    ci->callstatus &= ~CIST_HOOKED;
  }
}


static StkId adjust_varargs (lua_State *L, Proto *p, int actual) {
  int i;
  int nfixargs = p->numparams;
  StkId base, fixed;
  lua_assert(actual >= nfixargs);
  /* move fixed parameters to final position */
  fixed = L->top - actual;  /* first fixed argument */
  base = L->top;  /* final position of first argument */
  for (i=0; i<nfixargs; i++) {
    setobjs2s(L, L->top++, fixed + i);
    setnilvalue(fixed + i);
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



#define next_ci(L) (L->ci = (L->ci->next ? L->ci->next : luaE_extendCI(L)))


/*
** returns true if function has been executed (C function)
*/
int luaD_precall (lua_State *L, StkId func, int nresults) {
  LClosure *cl;
  ptrdiff_t funcr;
  if (!ttisfunction(func)) /* `func' is not a function? */
    func = tryfuncTM(L, func);  /* check the `function' tag method */
  funcr = savestack(L, func);
  cl = &clvalue(func)->l;
  L->ci->nresults = nresults;
  if (!cl->isC) {  /* Lua function? prepare its call */
    CallInfo *ci;
    int nparams, nargs;
    StkId base;
    Proto *p = cl->p;
    luaD_checkstack(L, p->maxstacksize);
    func = restorestack(L, funcr);
    nargs = cast_int(L->top - func) - 1;  /* number of real arguments */
    nparams = p->numparams;  /* number of expected parameters */
    for (; nargs < nparams; nargs++)
      setnilvalue(L->top++);  /* complete missing arguments */
    if (!p->is_vararg)  /* no varargs? */
      base = func + 1;
    else  /* vararg function */
      base = adjust_varargs(L, p, nargs);
    ci = next_ci(L);  /* now 'enter' new function */
    ci->func = func;
    ci->u.l.base = base;
    ci->top = base + p->maxstacksize;
    lua_assert(ci->top <= L->stack_last);
    ci->u.l.savedpc = p->code;  /* starting point */
    ci->u.l.tailcalls = 0;
    ci->callstatus = CIST_LUA;
    L->top = ci->top;
    if (L->hookmask & LUA_MASKCALL) {
      ci->u.l.savedpc++;  /* hooks assume 'pc' is already incremented */
      luaD_callhook(L, LUA_HOOKCALL, -1);
      ci->u.l.savedpc--;  /* correct 'pc' */
    }
    return 0;
  }
  else {  /* if is a C function, call it */
    CallInfo *ci;
    int n;
    luaD_checkstack(L, LUA_MINSTACK);  /* ensure minimum stack size */
    ci = next_ci(L);  /* now 'enter' new function */
    ci->func = restorestack(L, funcr);
    ci->top = L->top + LUA_MINSTACK;
    lua_assert(ci->top <= L->stack_last);
    ci->callstatus = 0;
    if (L->hookmask & LUA_MASKCALL)
      luaD_callhook(L, LUA_HOOKCALL, -1);
    lua_unlock(L);
    n = (*curr_func(L)->c.f)(L);  /* do the actual call */
    lua_lock(L);
    luaD_poscall(L, L->top - n);
    return 1;
  }
}


static StkId callrethooks (lua_State *L, StkId firstResult) {
  ptrdiff_t fr = savestack(L, firstResult);  /* next call may change stack */
  luaD_callhook(L, LUA_HOOKRET, -1);
  if (isLua(L->ci)) {  /* Lua function? */
    while ((L->hookmask & LUA_MASKRET) && L->ci->u.l.tailcalls--)
      luaD_callhook(L, LUA_HOOKTAILRET, -1);  /* ret. hooks for tail calls */
  }
  return restorestack(L, fr);
}


int luaD_poscall (lua_State *L, StkId firstResult) {
  StkId res;
  int wanted, i;
  CallInfo *ci = L->ci;
  if (L->hookmask & (LUA_MASKRET | LUA_MASKLINE)) {
    if (L->hookmask & LUA_MASKRET)
      firstResult = callrethooks(L, firstResult);
    L->oldpc = ci->previous->u.l.savedpc;  /* 'oldpc' for returning function */
  }
  res = ci->func;  /* res == final position of 1st result */
  L->ci = ci = ci->previous;  /* back to caller */
  wanted = ci->nresults;
  /* move results to correct place */
  for (i = wanted; i != 0 && firstResult < L->top; i--)
    setobjs2s(L, res++, firstResult++);
  while (i-- > 0)
    setnilvalue(res++);
  L->top = res;
  return (wanted - LUA_MULTRET);  /* 0 iff wanted == LUA_MULTRET */
}


/*
** Call a function (C or Lua). The function to be called is at *func.
** The arguments are on the stack, right after the function.
** When returns, all the results are on the stack, starting at the original
** function position.
*/
void luaD_call (lua_State *L, StkId func, int nResults, int allowyield) {
  global_State *g = G(L);
  if (++g->nCcalls >= LUAI_MAXCCALLS) {
    if (g->nCcalls == LUAI_MAXCCALLS)
      luaG_runerror(L, "C stack overflow");
    else if (g->nCcalls >= (LUAI_MAXCCALLS + (LUAI_MAXCCALLS>>3)))
      luaD_throw(L, LUA_ERRERR);  /* error while handing stack error */
  }
  if (!allowyield) L->nny++;
  if (!luaD_precall(L, func, nResults))  /* is a Lua function? */
    luaV_execute(L);  /* call it */
  if (!allowyield) L->nny--;
  g->nCcalls--;
  luaC_checkGC(L);
}


static void finishCcall (lua_State *L) {
  CallInfo *ci = L->ci;
  int n;
  lua_assert(ci->u.c.k != NULL);  /* must have a continuation */
  lua_assert(L->nny == 0);
  /* finish 'luaD_call' */
  G(L)->nCcalls--;
  /* finish 'lua_callk' */
  adjustresults(L, ci->nresults);
  /* call continuation function */
  if (!(ci->callstatus & CIST_STAT))  /* no call status? */
    ci->u.c.status = LUA_YIELD;  /* 'default' status */
  lua_assert(ci->u.c.status != LUA_OK);
  ci->callstatus = (ci->callstatus & ~(CIST_YPCALL | CIST_STAT)) | CIST_YIELDED;
  lua_unlock(L);
  n = (*ci->u.c.k)(L);
  lua_lock(L);
  /* finish 'luaD_precall' */
  luaD_poscall(L, L->top - n);
}


static void unroll (lua_State *L, void *ud) {
  UNUSED(ud);
  for (;;) {
    if (L->ci == &L->base_ci)  /* stack is empty? */
      return;  /* coroutine finished normally */
    if (!isLua(L->ci))  /* C function? */
      finishCcall(L);
    else {  /* Lua function */
      luaV_finishOp(L);  /* finish interrupted instruction */
      luaV_execute(L);  /* execute down to higher C 'boundary' */
    }
  }
}


static void resume (lua_State *L, void *ud) {
  StkId firstArg = cast(StkId, ud);
  CallInfo *ci = L->ci;
  if (L->status == LUA_OK) {  /* start coroutine? */
    lua_assert(ci == &L->base_ci);
    if (!luaD_precall(L, firstArg - 1, LUA_MULTRET))  /* Lua function? */
      luaV_execute(L);  /* call it */
  }
  else {  /* resuming from previous yield */
    lua_assert(L->status == LUA_YIELD);
    L->status = LUA_OK;
    if (isLua(ci))  /* yielded inside a hook? */
      luaV_execute(L);
    else {  /* 'common' yield */
      if (ci->u.c.k != NULL) {  /* does it have a continuation? */
        int n;
        ci->u.c.status = LUA_YIELD;  /* 'default' status */
        ci->callstatus |= CIST_YIELDED;
        ci->func = restorestack(L, ci->u.c.oldtop);
        lua_unlock(L);
        n = (*ci->u.c.k)(L);  /* call continuation */
        lua_lock(L);
        firstArg = L->top - n;
      }
      G(L)->nCcalls--;  /* finish 'luaD_call' */
      luaD_poscall(L, firstArg);  /* finish 'luaD_precall' */
    }
    unroll(L, NULL);
  }
}


static int resume_error (lua_State *L, const char *msg) {
  L->top = L->ci->func + 1;
  setsvalue2s(L, L->top, luaS_new(L, msg));
  incr_top(L);
  lua_unlock(L);
  return LUA_ERRRUN;
}


/*
** check whether thread has a suspended protected call
*/
static CallInfo *findpcall (lua_State *L) {
  CallInfo *ci;
  for (ci = L->ci; ci != NULL; ci = ci->previous) {  /* search for a pcall */
    if (ci->callstatus & CIST_YPCALL)
      return ci;
  }
  return NULL;  /* no pending pcall */
}


static int recover (lua_State *L, int status) {
  StkId oldtop;
  CallInfo *ci = findpcall(L);
  if (ci == NULL) return 0;  /* no recovery point */
  /* "finish" luaD_pcall */
  oldtop = restorestack(L, ci->u.c.oldtop);
  luaF_close(L, oldtop);
  luaD_seterrorobj(L, status, oldtop);
  L->ci = ci;
  L->allowhook = ci->u.c.old_allowhook;
  L->nny = 0;  /* should be zero to be yieldable */
  luaD_shrinkstack(L);
  L->errfunc = ci->u.c.old_errfunc;
  ci->callstatus |= CIST_STAT;  /* call has error status */
  ci->u.c.status = status;  /* (here it is) */
  return 1;  /* continue running the coroutine */
}


LUA_API int lua_resume (lua_State *L, int nargs) {
  int status;
  lua_lock(L);
  if (L->status != LUA_YIELD) {
    if (L->status != LUA_OK)
      return resume_error(L, "cannot resume dead coroutine");
    else if (L->ci != &L->base_ci)
      return resume_error(L, "cannot resume non-suspended coroutine");
  }
  luai_userstateresume(L, nargs);
  if (G(L)->nCcalls >= LUAI_MAXCCALLS)
      return resume_error(L, "C stack overflow");
  ++G(L)->nCcalls;  /* count resume */
  L->nny = 0;  /* allow yields */
  status = luaD_rawrunprotected(L, resume, L->top - nargs);
  while (status != LUA_OK && status != LUA_YIELD) {  /* error? */
    if (recover(L, status))  /* recover point? */
      status = luaD_rawrunprotected(L, unroll, NULL);  /* run continuation */
    else {  /* unrecoverable error */
      L->status = cast_byte(status);  /* mark thread as `dead' */
      luaD_seterrorobj(L, status, L->top);
      L->ci->top = L->top;
      break;
    }
  }
  lua_assert(status == L->status);
  L->nny = 1;  /* do not allow yields */
  --G(L)->nCcalls;
  lua_unlock(L);
  return status;
}

LUA_API int lua_yieldk (lua_State *L, int nresults, int ctx, lua_CFunction k) {
  CallInfo *ci = L->ci;
  luai_userstateyield(L, nresults);
  lua_lock(L);
  if (L->nny > 0)
    luaG_runerror(L, "attempt to yield across metamethod/C-call boundary");
  L->status = LUA_YIELD;
  if (isLua(ci)) {  /* inside a hook? */
    api_check(L, k == NULL, "hooks cannot continue after yielding");
  }
  else {
    if ((ci->u.c.k = k) != NULL) {  /* is there a continuation? */
      ci->u.c.ctx = ctx;  /* save context */
      ci->u.c.oldtop = savestack(L, ci->func);  /* save current 'func' */
    }
    ci->func = L->top - nresults - 1;  /* protect stack slots below */
    luaD_throw(L, LUA_YIELD);
  }
  lua_assert(ci->callstatus & CIST_HOOKED);  /* must be inside a hook */
  lua_unlock(L);
  return 0;  /* otherwise, return to 'luaD_callhook' */
}


int luaD_pcall (lua_State *L, Pfunc func, void *u,
                ptrdiff_t old_top, ptrdiff_t ef) {
  int status;
  CallInfo *old_ci = L->ci;
  lu_byte old_allowhooks = L->allowhook;
  unsigned short old_nny = L->nny;
  ptrdiff_t old_errfunc = L->errfunc;
  L->errfunc = ef;
  status = luaD_rawrunprotected(L, func, u);
  if (status != LUA_OK) {  /* an error occurred? */
    StkId oldtop = restorestack(L, old_top);
    luaF_close(L, oldtop);  /* close possible pending closures */
    luaD_seterrorobj(L, status, oldtop);
    L->ci = old_ci;
    L->allowhook = old_allowhooks;
    L->nny = old_nny;
    luaD_shrinkstack(L);
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
  Varlist varl;  /* list of local variables (to be used by the parser) */
  const char *name;
};

static void f_parser (lua_State *L, void *ud) {
  int i;
  Proto *tf;
  Closure *cl;
  struct SParser *p = cast(struct SParser *, ud);
  int c = luaZ_lookahead(p->z);
  luaC_checkGC(L);
  tf = (c == LUA_SIGNATURE[0])
           ? luaU_undump(L, p->z, &p->buff, p->name)
           : luaY_parser(L, p->z, &p->buff, &p->varl, p->name);
  setptvalue2s(L, L->top, tf);
  incr_top(L);
  cl = luaF_newLclosure(L, tf->sizeupvalues, hvalue(gt(L)));
  cl->l.p = tf;
  setclvalue(L, L->top - 1, cl);
  for (i = 0; i < tf->sizeupvalues; i++)  /* initialize upvalues */
    cl->l.upvals[i] = luaF_newupval(L);
}


int luaD_protectedparser (lua_State *L, ZIO *z, const char *name) {
  struct SParser p;
  int status;
  p.z = z; p.name = name;
  p.varl.actvar = NULL; p.varl.nactvar = p.varl.actvarsize = 0;
  luaZ_initbuffer(L, &p.buff);
  status = luaD_pcall(L, f_parser, &p, savestack(L, L->top), L->errfunc);
  luaZ_freebuffer(L, &p.buff);
  luaM_freearray(L, p.varl.actvar, p.varl.actvarsize);
  return status;
}


