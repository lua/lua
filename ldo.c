/*
** $Id: ldo.c,v 1.1 2001/11/29 22:14:34 rieru Exp rieru $
** Stack and Call structure of Lua
** See Copyright Notice in lua.h
*/


#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
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


/* space to handle stack overflow errors */
#define EXTRA_STACK   (2*LUA_MINSTACK)


static void restore_stack_limit (lua_State *L) {
  StkId limit = L->stack+(L->stacksize-EXTRA_STACK)-1;
  if (L->top < limit)
    L->stack_last = limit;
}


void luaD_init (lua_State *L, int stacksize) {
  stacksize += EXTRA_STACK;
  L->stack = luaM_newvector(L, stacksize, TObject);
  L->stacksize = stacksize;
  L->top = L->stack + RESERVED_STACK_PREFIX;
  restore_stack_limit(L);
  luaM_reallocvector(L, L->base_ci, 0, 20, CallInfo);
  L->ci = L->base_ci;
  L->ci->base = L->top;
  L->ci->savedpc = NULL;
  L->ci->pc = NULL;
  L->size_ci = 20;
  L->end_ci = L->base_ci + L->size_ci;
}


void luaD_stackerror (lua_State *L) {
  if (L->stack_last == L->stack+L->stacksize-1) {
    /* overflow while handling overflow */
    luaD_breakrun(L, LUA_ERRERR);  /* break run without error message */
  }
  else {
    L->stack_last += EXTRA_STACK;  /* to be used by error message */
    lua_assert(L->stack_last == L->stack+L->stacksize-1);
    luaD_error(L, "stack overflow");
  }
}


/*
** adjust top to new value; assume that new top is valid
*/
void luaD_adjusttop (lua_State *L, StkId newtop) {
  while (L->top < newtop)
    setnilvalue(L->top++);
  L->top = newtop;  /* `newtop' could be lower than `top' */
}


/*
** Open a hole inside the stack at `pos'
*/
static void luaD_openstack (lua_State *L, StkId pos) {
  int i = L->top-pos; 
  while (i--) setobj(pos+i+1, pos+i);
  incr_top;
}


static void dohook (lua_State *L, lua_Debug *ar, lua_Hook hook) {
  StkId old_top = L->top;
  luaD_checkstack(L, LUA_MINSTACK);  /* ensure minimum stack size */
  L->allowhooks = 0;  /* cannot call hooks inside a hook */
  lua_unlock(L);
  (*hook)(L, ar);
  lua_lock(L);
  lua_assert(L->allowhooks == 0);
  L->allowhooks = 1;
  L->top = old_top;
}


void luaD_lineHook (lua_State *L, int line, lua_Hook linehook) {
  if (L->allowhooks) {
    lua_Debug ar;
    ar.event = "line";
    ar._ci = L->ci - L->base_ci;
    ar.currentline = line;
    dohook(L, &ar, linehook);
  }
}


void luaD_callHook (lua_State *L, lua_Hook callhook, const char *event) {
  if (L->allowhooks) {
    lua_Debug ar;
    ar.event = event;
    ar._ci = L->ci - L->base_ci;
    dohook(L, &ar, callhook);
  }
}


#define newci(L)	((++L->ci == L->end_ci) ? growci(L) : L->ci)

static CallInfo *growci (lua_State *L) {
  lua_assert(L->ci == L->end_ci);
  luaM_reallocvector(L, L->base_ci, L->size_ci, 2*L->size_ci, CallInfo);
  L->ci = L->base_ci + L->size_ci;
  L->size_ci *= 2;
  L->end_ci = L->base_ci + L->size_ci;
  return L->ci;
}


StkId luaD_precall (lua_State *L, StkId func) {
  CallInfo *ci;
  int n;
  if (ttype(func) != LUA_TFUNCTION) {
    /* `func' is not a function; check the `function' tag method */
    const TObject *tm = luaT_gettmbyobj(L, func, TM_CALL);
    if (ttype(tm) != LUA_TFUNCTION)
      luaG_typeerror(L, func, "call");
    luaD_openstack(L, func);
    setobj(func, tm);  /* tag method is the new function to be called */
  }
  lua_assert(ttype(func) == LUA_TFUNCTION);
  ci = newci(L);
  ci->base = func+1;
  ci->savedpc = NULL;
  ci->pc = NULL;
  if (L->callhook)
    luaD_callHook(L, L->callhook, "call");
  if (!clvalue(func)->c.isC) return NULL;
  /* if is a C function, call it */
  luaD_checkstack(L, LUA_MINSTACK);  /* ensure minimum stack size */
  lua_unlock(L);
#if LUA_COMPATUPVALUES
  lua_pushupvalues(L);
#endif
  n = (*clvalue(func)->c.f)(L);  /* do the actual call */
  lua_lock(L);
  return L->top - n;
}


void luaD_poscall (lua_State *L, int wanted, StkId firstResult) { 
  StkId res;
  if (L->callhook)
    luaD_callHook(L, L->callhook, "return");
  res = L->ci->base - 1;  /* `func' = final position of 1st result */
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
  if (firstResult == NULL)  /* is a Lua function? */
    firstResult = luaV_execute(L, &clvalue(func)->l, func+1);  /* call it */
  luaD_poscall(L, nResults, firstResult);
}


/*
** Execute a protected call.
*/
struct CallS {  /* data to `f_call' */
  StkId func;
  int nresults;
};

static void f_call (lua_State *L, void *ud) {
  struct CallS *c = cast(struct CallS *, ud);
  luaD_call(L, c->func, c->nresults);
}


LUA_API int lua_call (lua_State *L, int nargs, int nresults) {
  StkId func;
  struct CallS c;
  int status;
  lua_lock(L);
  func = L->top - (nargs+1);  /* function to be called */
  c.func = func; c.nresults = nresults;
  status = luaD_runprotected(L, f_call, &c);
  if (status != 0) {  /* an error occurred? */
    luaF_close(L, func);  /* close eventual pending closures */
    L->top = func;  /* remove parameters from the stack */
  }
  lua_unlock(L);
  return status;
}


/*
** Execute a protected parser.
*/
struct SParser {  /* data to `f_parser' */
  ZIO *z;
  int bin;
};

static void f_parser (lua_State *L, void *ud) {
  struct SParser *p = cast(struct SParser *, ud);
  Proto *tf = p->bin ? luaU_undump(L, p->z) : luaY_parser(L, p->z);
  Closure *cl = luaF_newLclosure(L, 0);
  cl->l.p = tf;
  setclvalue(L->top, cl);
  incr_top;
}


static int protectedparser (lua_State *L, ZIO *z, int bin) {
  struct SParser p;
  lu_mem old_blocks;
  int status;
  lua_lock(L);
  p.z = z; p.bin = bin;
  /* before parsing, give a (good) chance to GC */
  if (G(L)->nblocks/8 >= G(L)->GCthreshold/10)
    luaC_collectgarbage(L);
  old_blocks = G(L)->nblocks;
  status = luaD_runprotected(L, f_parser, &p);
  if (status == 0) {
    /* add new memory to threshold (as it probably will stay) */
    lua_assert(G(L)->nblocks >= old_blocks);
    G(L)->GCthreshold += (G(L)->nblocks - old_blocks);
  }
  else if (status == LUA_ERRRUN)  /* an error occurred: correct error code */
    status = LUA_ERRSYNTAX;
  lua_unlock(L);
  return status;
}


LUA_API int lua_loadfile (lua_State *L, const char *filename) {
  ZIO z;
  int status;
  int bin;  /* flag for file mode */
  int nlevel;  /* level on the stack of filename */
  FILE *f = (filename == NULL) ? stdin : fopen(filename, "r");
  if (f == NULL) return LUA_ERRFILE;  /* unable to open file */
  bin = (ungetc(getc(f), f) == LUA_SIGNATURE[0]);
  if (bin && f != stdin) {
    fclose(f);
    f = fopen(filename, "rb");  /* reopen in binary mode */
    if (f == NULL) return LUA_ERRFILE;  /* unable to reopen file */
  }
  lua_pushliteral(L, "@");
  lua_pushstring(L, (filename == NULL) ? "(stdin)" : filename);
  lua_concat(L, 2);
  nlevel = lua_gettop(L);
  filename = lua_tostring(L, -1);  /* filename = `@'..filename */
  luaZ_Fopen(&z, f, filename);
  status = protectedparser(L, &z, bin);
  lua_remove(L, nlevel);  /* remove filename */
  if (f != stdin)
    fclose(f);
  return status;
}


LUA_API int lua_loadbuffer (lua_State *L, const char *buff, size_t size,
                          const char *name) {
  ZIO z;
  if (!name) name = "?";
  luaZ_mopen(&z, buff, size, name);
  return protectedparser(L, &z, buff[0]==LUA_SIGNATURE[0]);
}



/*
** {======================================================
** Error-recovery functions (based on long jumps)
** =======================================================
*/

/* chain list of long jump buffers */
struct lua_longjmp {
  jmp_buf b;
  struct lua_longjmp *previous;
  volatile int status;  /* error code */
  int ci;  /* index of call info of active function that set protection */
  StkId top;  /* top stack when protection was set */
  int allowhooks;  /* `allowhook' state when protection was set */
};


static void message (lua_State *L, const char *s) {
  TObject o, m;
  setsvalue(&o, luaS_newliteral(L, LUA_ERRORMESSAGE));
  luaV_gettable(L, gt(L), &o, &m);
  if (ttype(&m) == LUA_TFUNCTION) {
    StkId top = L->top;
    setobj(top, &m);
    incr_top;
    setsvalue(top+1, luaS_new(L, s));
    incr_top;
    luaD_call(L, top, 0);
  }
}


/*
** Reports an error, and jumps up to the available recovery label
*/
void luaD_error (lua_State *L, const char *s) {
  if (s) message(L, s);
  luaD_breakrun(L, LUA_ERRRUN);
}


void luaD_breakrun (lua_State *L, int errcode) {
  if (L->errorJmp) {
    L->errorJmp->status = errcode;
    longjmp(L->errorJmp->b, 1);
  }
  else {
    if (errcode != LUA_ERRMEM && errcode != LUA_ERRERR)
      message(L, "unable to recover; exiting\n");
    exit(EXIT_FAILURE);
  }
}


int luaD_runprotected (lua_State *L, void (*f)(lua_State *, void *), void *ud) {
  struct lua_longjmp lj;
  lj.ci = L->ci - L->base_ci;
  lj.top = L->top;
  lj.allowhooks = L->allowhooks;
  lj.status = 0;
  lj.previous = L->errorJmp;  /* chain new error handler */
  L->errorJmp = &lj;
  if (setjmp(lj.b) == 0)
    (*f)(L, ud);
  else {  /* an error occurred: restore the state */
    L->ci = L->base_ci + lj.ci;
    L->top = lj.top;
    L->allowhooks = lj.allowhooks;
    restore_stack_limit(L);
  }
  L->errorJmp = lj.previous;  /* restore old error handler */
  return lj.status;
}

/* }====================================================== */

