/*
** $Id: ldo.c,v 1.177 2002/05/27 20:35:40 roberto Exp roberto $
** Stack and Call structure of Lua
** See Copyright Notice in lua.h
*/


#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

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



/* chain list of long jump buffers */
struct lua_longjmp {
  struct lua_longjmp *previous;
  CallInfo *ci;  /* index of call info of active function that set protection */
  StkId top;  /* top stack when protection was set */
  jmp_buf b;
  int allowhooks;  /* `allowhook' state when protection was set */
  volatile int status;  /* error code */
  TObject *err;  /* error function -> message (start of `ud') */
};



static void correctstack (lua_State *L, TObject *oldstack) {
  struct lua_longjmp *lj;
  CallInfo *ci;
  UpVal *up;
  L->top = (L->top - oldstack) + L->stack;
  for (lj = L->errorJmp; lj != NULL; lj = lj->previous)
    lj->top = (lj->top - oldstack) + L->stack;
  for (up = L->openupval; up != NULL; up = up->next)
    up->v = (up->v - oldstack) + L->stack;
  for (ci = L->base_ci; ci <= L->ci; ci++) {
    ci->base = (ci->base - oldstack) + L->stack;
    ci->top = (ci->top - oldstack) + L->stack;
    if (ci->pc) {  /* entry is of an active Lua function? */
      if (ci->pc != (ci-1)->pc)
        *ci->pb = (*ci->pb - oldstack) + L->stack;
    }
  }
}


void luaD_reallocstack (lua_State *L, int newsize) {
  TObject *oldstack = L->stack;
  luaM_reallocvector(L, L->stack, L->stacksize, newsize, TObject);
  L->stacksize = newsize;
  L->stack_last = L->stack+(newsize-EXTRA_STACK)-1;
  correctstack(L, oldstack);
}


static void correctCI (lua_State *L, CallInfo *oldci) {
  struct lua_longjmp *lj;
  for (lj = L->errorJmp; lj != NULL; lj = lj->previous) {
    lj->ci = (lj->ci - oldci) + L->base_ci;
  }
}


void luaD_reallocCI (lua_State *L, int newsize) {
  CallInfo *oldci = L->base_ci;
  luaM_reallocvector(L, L->base_ci, L->size_ci, newsize, CallInfo);
  L->size_ci = newsize;
  L->ci = (L->ci - oldci) + L->base_ci;
  L->end_ci = L->base_ci + L->size_ci;
  correctCI(L, oldci);
}


static void restore_stack_limit (lua_State *L) {
  if (L->size_ci > LUA_MAXCALLS) {  /* there was an overflow? */
    int inuse = (L->ci - L->base_ci);
    if (inuse + 1 < LUA_MAXCALLS)  /* can `undo' overflow? */
      luaD_reallocCI(L, LUA_MAXCALLS);
  }
}


void luaD_growstack (lua_State *L, int n) {
  if (n <= L->stacksize)  /* double size is enough? */
    luaD_reallocstack(L, 2*L->stacksize);
  else
    luaD_reallocstack(L, L->stacksize + n + EXTRA_STACK);
}


static void luaD_growCI (lua_State *L) {
  L->ci--;
  if (L->size_ci > LUA_MAXCALLS)  /* overflow while handling overflow? */
    luaD_error(L, "error in error handling", LUA_ERRERR);
  else {
    luaD_reallocCI(L, 2*L->size_ci);
    if (L->size_ci > LUA_MAXCALLS)
      luaG_runerror(L, "stack overflow");
  }
  L->ci++;
}


/*
** Open a hole inside the stack at `pos'
*/
static void luaD_openstack (lua_State *L, StkId pos) {
  StkId p;
  for (p = L->top; p > pos; p--) setobj(p, p-1);
  incr_top(L);
}


static void dohook (lua_State *L, lua_Debug *ar, lua_Hook hook) {
  ptrdiff_t top = savestack(L, L->top);
  ptrdiff_t ci_top = savestack(L, L->ci->top);
  luaD_checkstack(L, LUA_MINSTACK);  /* ensure minimum stack size */
  L->ci->top = L->top + LUA_MINSTACK;
  L->allowhooks = 0;  /* cannot call hooks inside a hook */
  lua_unlock(L);
  (*hook)(L, ar);
  lua_lock(L);
  lua_assert(L->allowhooks == 0);
  L->allowhooks = 1;
  L->ci->top = restorestack(L, ci_top);
  L->top = restorestack(L, top);
}


void luaD_lineHook (lua_State *L, int line) {
  if (L->allowhooks) {
    lua_Debug ar;
    ar.event = "line";
    ar.i_ci = L->ci - L->base_ci;
    ar.currentline = line;
    dohook(L, &ar, L->linehook);
  }
}


static void luaD_callHook (lua_State *L, lua_Hook callhook, const char *event) {
  if (L->allowhooks) {
    lua_Debug ar;
    ar.event = event;
    ar.i_ci = L->ci - L->base_ci;
    L->ci->pc = NULL;  /* function is not active */
    L->ci->top = L->ci->base;  /* `top' may not have a valid value yet */ 
    dohook(L, &ar, callhook);
  }
}


static void adjust_varargs (lua_State *L, int nfixargs) {
  int i;
  Table *htab;
  TObject nname;
  int actual = L->top - L->ci->base;  /* actual number of arguments */
  if (actual < nfixargs) {
    luaD_checkstack(L, nfixargs - actual);
    for (; actual < nfixargs; ++actual)
      setnilvalue(L->top++);
  }
  actual -= nfixargs;  /* number of extra arguments */
  htab = luaH_new(L, 0, 0);  /* create `arg' table */
  for (i=0; i<actual; i++)  /* put extra arguments into `arg' table */
    setobj(luaH_setnum(L, htab, i+1), L->top - actual + i);
  /* store counter in field `n' */
  setsvalue(&nname, luaS_newliteral(L, "n"));
  setnvalue(luaH_set(L, htab, &nname), actual);
  L->top -= actual;  /* remove extra elements from the stack */
  sethvalue(L->top, htab);
  incr_top(L);
}


static StkId tryfuncTM (lua_State *L, StkId func) {
  const TObject *tm = luaT_gettmbyobj(L, func, TM_CALL);
  if (ttype(tm) != LUA_TFUNCTION) {
    L->ci--;  /* undo increment (no function here) */
    luaG_typeerror(L, func, "call");
  }
  luaD_openstack(L, func);
  func = L->ci->base - 1;  /* previous call may change stack */
  setobj(func, tm);  /* tag method is the new function to be called */
  return func;
}


StkId luaD_precall (lua_State *L, StkId func) {
  CallInfo *ci;
  LClosure *cl;
  if (++L->ci == L->end_ci) luaD_growCI(L);
  ci = L->ci;
  ci->base = func+1;
  ci->pc = NULL;
  if (ttype(func) != LUA_TFUNCTION) /* `func' is not a function? */
    func = tryfuncTM(L, func);  /* check the `function' tag method */
  cl = &clvalue(func)->l;
  if (L->callhook) {
    luaD_callHook(L, L->callhook, "call");
    ci = L->ci;  /* previous call may realocate `ci' */
  }
  if (!cl->isC) {  /* Lua function? prepare its call */
    Proto *p = cl->p;
    ci->savedpc = p->code;  /* starting point */
    if (p->is_vararg)  /* varargs? */
      adjust_varargs(L, p->numparams);
    luaD_checkstack(L, p->maxstacksize);
    ci->top = ci->base + p->maxstacksize;
    while (L->top < ci->top)
      setnilvalue(L->top++);
    L->top = ci->top;
    return NULL;
  }
  else {  /* if is a C function, call it */
    int n;
    luaD_checkstack(L, LUA_MINSTACK);  /* ensure minimum stack size */
    ci->top = L->top + LUA_MINSTACK;
    lua_unlock(L);
#if LUA_COMPATUPVALUES
    lua_pushupvalues(L);
#endif
    n = (*clvalue(ci->base-1)->c.f)(L);  /* do the actual call */
    lua_lock(L);
    return L->top - n;
  }
}


void luaD_poscall (lua_State *L, int wanted, StkId firstResult) { 
  StkId res;
  if (L->callhook) {
    ptrdiff_t fr = savestack(L, firstResult);  /* next call may change stack */
    luaD_callHook(L, L->callhook, "return");
    firstResult = restorestack(L, fr);
  }
  res = L->ci->base - 1;  /* res == final position of 1st result */
  L->ci--;
  /* move results to correct place */
  while (wanted != 0 && firstResult < L->top) {
    setobj(res++, firstResult++);
    wanted--;
  }
  while (wanted-- > 0)
    setnilvalue(res++);
  L->top = res;
  luaC_checkGC(L);
}


/*
** Call a function (C or Lua). The function to be called is at *func.
** The arguments are on the stack, right after the function.
** When returns, all the results are on the stack, starting at the original
** function position.
*/ 
void luaD_call (lua_State *L, StkId func, int nResults) {
  StkId firstResult = luaD_precall(L, func);
  if (firstResult == NULL) {  /* is a Lua function? */
    firstResult = luaV_execute(L);  /* call it */
    if (firstResult == NULL) {
      luaD_poscall(L, 0, L->top);
      luaG_runerror(L, "attempt to `yield' across tag-method/C-call boundary");
    }
  }
  luaD_poscall(L, nResults, firstResult);
}


LUA_API void lua_cobegin (lua_State *L, int nargs) {
  lua_lock(L);
  luaD_precall(L, L->top - (nargs+1));
  lua_unlock(L);
}


static void move_results (lua_State *L, TObject *from, TObject *to) {
  while (from < to) {
    setobj(L->top, from);
    from++;
    incr_top(L);
  }
}


struct ResS {
  TObject err;
  int numres;
};

static void resume (lua_State *L, void *ud) {
  StkId firstResult;
  CallInfo *ci = L->ci;
  if (ci->savedpc != ci_func(ci)->l.p->code) {  /* not first time? */
    /* finish interupted execution of `OP_CALL' */
    int nresults;
    lua_assert(GET_OPCODE(*((ci-1)->savedpc - 1)) == OP_CALL);
    nresults = GETARG_C(*((ci-1)->savedpc - 1)) - 1;
    luaD_poscall(L, nresults, L->top);  /* complete it */
    if (nresults >= 0) L->top = L->ci->top;
  }
  firstResult = luaV_execute(L);
  if (firstResult == NULL)   /* yield? */
    cast(struct ResS *, ud)->numres = L->ci->yield_results;
  else {  /* return */
    cast(struct ResS *, ud)->numres = L->top - firstResult;
    luaD_poscall(L, LUA_MULTRET, firstResult);  /* finalize this coroutine */
  }
}


LUA_API int lua_resume (lua_State *L, lua_State *co) {
  CallInfo *ci;
  struct ResS ud;
  int status;
  lua_lock(L);
  ci = co->ci;
  if (ci == co->base_ci)  /* no activation record? ?? */
    luaG_runerror(L, "thread is dead - cannot be resumed");
  if (co->errorJmp != NULL)  /* ?? */
    luaG_runerror(L, "thread is active - cannot be resumed");
  if (L->errorJmp) {
    setobj(&ud.err, L->errorJmp->err);
  }
  else
    setnilvalue(&ud.err);
  status = luaD_runprotected(co, resume, &ud.err);
  if (status == 0)  
    move_results(L, co->top - ud.numres, co->top);
  else {
    setobj(L->top++, &ud.err);
}
  lua_unlock(L);
  return status;
}


LUA_API int lua_yield (lua_State *L, int nresults) {
  CallInfo *ci;
  lua_lock(L);
  ci = L->ci;
  if (ci_func(ci-1)->c.isC)
    luaG_runerror(L, "cannot `yield' a C function");
  ci->yield_results = nresults;
  lua_unlock(L);
  return -1;
}


/*
** Execute a protected call.
*/
struct CallS {  /* data to `f_call' */
  TObject err;  /* error field... */
  StkId func;
  int nresults;
};


static void f_call (lua_State *L, void *ud) {
  struct CallS *c = cast(struct CallS *, ud);
  luaD_call(L, c->func, c->nresults);
}


int luaD_pcall (lua_State *L, int nargs, int nresults, const TObject *err) {
  struct CallS c;
  int status;
  c.func = L->top - (nargs+1);  /* function to be called */
  c.nresults = nresults;
  c.err = *err;
  status = luaD_runprotected(L, &f_call, &c.err);
  if (status != 0) {  /* an error occurred? */
    L->top -= nargs+1;  /* remove parameters and func from the stack */
    luaF_close(L, L->top);  /* close eventual pending closures */
    setobj(L->top++, &c.err);
  }
  return status;
}


/*
** Execute a protected parser.
*/
struct SParser {  /* data to `f_parser' */
  TObject err;  /* error field... */
  ZIO *z;
  int bin;
};

static void f_parser (lua_State *L, void *ud) {
  struct SParser *p = cast(struct SParser *, ud);
  Proto *tf = p->bin ? luaU_undump(L, p->z) : luaY_parser(L, p->z);
  Closure *cl = luaF_newLclosure(L, 0);
  cl->l.p = tf;
  setclvalue(L->top, cl);
  incr_top(L);
}


int luaD_protectedparser (lua_State *L, ZIO *z, int bin) {
  struct SParser p;
  lu_mem old_blocks;
  int status;
  p.z = z; p.bin = bin;
  /* before parsing, give a (good) chance to GC */
  if (G(L)->nblocks + G(L)->nblocks/4 >= G(L)->GCthreshold)
    luaC_collectgarbage(L);
  old_blocks = G(L)->nblocks;
  setnilvalue(&p.err);
  status = luaD_runprotected(L, f_parser, &p.err);
  if (status == 0) {
    /* add new memory to threshold (as it probably will stay) */
    lua_assert(G(L)->nblocks >= old_blocks);
    G(L)->GCthreshold += (G(L)->nblocks - old_blocks);
  }
  else {
    setobj(L->top++, &p.err);
    lua_assert(status != LUA_ERRRUN);
  }
  return status;
}



/*
** {======================================================
** Error-recovery functions (based on long jumps)
** =======================================================
*/


static void message (lua_State *L, const TObject *msg, int nofunc) {
  TObject *m = L->errorJmp->err;
  if (nofunc || ttype(m) != LUA_TFUNCTION) {  /* no error function? */
    setobj(m, msg);  /* keep error message */
  }
  else {  /* call error function */
    setobj(L->top, m);
    setobj(L->top + 1, msg);
    L->top += 2;
    luaD_call(L, L->top - 2, 1);
    setobj(m, L->top - 1);
  }
}


/*
** Reports an error, and jumps up to the available recovery label
*/
void luaD_errorobj (lua_State *L, const TObject *s, int errcode) {
  if (L->errorJmp) {
    L->errorJmp->status = errcode;
    message(L, s, (errcode >= LUA_ERRMEM));
    longjmp(L->errorJmp->b, 1);
  }
  else {
    G(L)->panic(L);
    exit(EXIT_FAILURE);
  }
}


void luaD_error (lua_State *L, const char *s, int errcode) {
  TObject errobj;
  if (errcode == LUA_ERRMEM && (G(L) == NULL || G(L)->GCthreshold == 0))
    setnilvalue(&errobj);  /* error bulding state */
  else
    setsvalue(&errobj, luaS_new(L, s));
  luaD_errorobj(L, &errobj, errcode);
}


int luaD_runprotected (lua_State *L, Pfunc f, TObject *ud) {
  struct lua_longjmp lj;
  lj.ci = L->ci;
  lj.top = L->top;
  lj.allowhooks = L->allowhooks;
  lj.status = 0;
  lj.err = ud;
  lj.previous = L->errorJmp;  /* chain new error handler */
  L->errorJmp = &lj;
  if (setjmp(lj.b) == 0)
    (*f)(L, ud);
  else {  /* an error occurred */
    L->ci = lj.ci;  /* restore the state */
    L->top = lj.top;
    L->allowhooks = lj.allowhooks;
    restore_stack_limit(L);
  }
  L->errorJmp = lj.previous;  /* restore old error handler */
  return lj.status;
}

/* }====================================================== */

