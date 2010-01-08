/*
** $Id: lstate.c,v 2.68 2009/12/22 15:32:50 roberto Exp $
** Global State
** See Copyright Notice in lua.h
*/


#include <stddef.h>

#define lstate_c
#define LUA_CORE

#include "lua.h"

#include "lapi.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "llex.h"
#include "lmem.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"


#if !defined(LUAI_GCPAUSE)
#define LUAI_GCPAUSE	162  /* 162% (wait memory to double before next GC) */
#endif

#if !defined(LUAI_GCMUL)
#define LUAI_GCMUL	200 /* GC runs 'twice the speed' of memory allocation */
#endif


/*
** thread state + extra space
*/
typedef struct LX {
#if defined(LUAI_EXTRASPACE)
  char buff[LUAI_EXTRASPACE];
#endif
  lua_State l;
} LX;


/*
** Main thread combines a thread state and the global state
*/
typedef struct LG {
  LX l;
  global_State g;
} LG;



#define fromstate(L)	(cast(LX *, cast(lu_byte *, (L)) - offsetof(LX, l)))



/*
** maximum number of nested calls made by error-handling function
*/
#define LUAI_EXTRACALLS		10


CallInfo *luaE_extendCI (lua_State *L) {
  CallInfo *ci = luaM_new(L, CallInfo);
  lua_assert(L->ci->next == NULL);
  L->ci->next = ci;
  ci->previous = L->ci;
  ci->next = NULL;
  return ci;
}


void luaE_freeCI (lua_State *L) {
  CallInfo *ci = L->ci;
  CallInfo *next = ci->next;
  ci->next = NULL;
  while ((ci = next) != NULL) {
    next = ci->next;
    luaM_free(L, ci);
  }
}


static void stack_init (lua_State *L1, lua_State *L) {
  int i;
  /* initialize stack array */
  L1->stack = luaM_newvector(L, BASIC_STACK_SIZE, TValue);
  L1->stacksize = BASIC_STACK_SIZE;
  for (i = 0; i < BASIC_STACK_SIZE; i++)
    setnilvalue(L1->stack + i);  /* erase new stack */
  L1->top = L1->stack;
  L1->stack_last = L1->stack + L1->stacksize - EXTRA_STACK;
  /* initialize first ci */
  L1->ci->func = L1->top;
  setnilvalue(L1->top++);  /* 'function' entry for this 'ci' */
  L1->ci->top = L1->top + LUA_MINSTACK;
  L1->ci->callstatus = 0;
}


static void freestack (lua_State *L) {
  L->ci = &L->base_ci;  /* reset 'ci' list */
  luaE_freeCI(L);
  luaM_freearray(L, L->stack, L->stacksize);
}


/*
** Calls the function in variable pointed to by userdata in first argument
** (Userdata cannot point directly to the function because pointer to
** function is not compatible with void*.)
*/
static int cpcall (lua_State *L) {
  lua_CFunction f = *(lua_CFunction *)lua_touserdata(L, 1);
  lua_remove(L, 1);  /* remove f from stack */
  /* restore original environment for 'cpcall' */
  lua_pushglobaltable(L);
  lua_replace(L, LUA_ENVIRONINDEX);
  return f(L);
}


/*
** Create registry table and its predefined values
*/
static void init_registry (lua_State *L, global_State *g) {
  Closure *cp;
  TValue mt;
  /* create registry */
  Table *registry = luaH_new(L);
  sethvalue(L, &g->l_registry, registry);
  luaH_resize(L, registry, LUA_RIDX_LAST, 0);
  /* registry[LUA_RIDX_MAINTHREAD] = L */
  setthvalue(L, &mt, L);
  setobj2t(L, luaH_setint(L, registry, LUA_RIDX_MAINTHREAD), &mt);
  /* registry[LUA_RIDX_CPCALL] = cpcall */
  cp = luaF_newCclosure(L, 0, g->l_gt);
  cp->c.f = cpcall;
  setclvalue(L, &mt, cp);
  setobj2t(L, luaH_setint(L, registry, LUA_RIDX_CPCALL), &mt);
  /* registry[LUA_RIDX_GLOBALS] = l_gt */
  sethvalue(L, &mt, g->l_gt);
  setobj2t(L, luaH_setint(L, registry, LUA_RIDX_GLOBALS), &mt);
}


/*
** open parts of the state that may cause memory-allocation errors
*/
static void f_luaopen (lua_State *L, void *ud) {
  global_State *g = G(L);
  UNUSED(ud);
  stack_init(L, L);  /* init stack */
  g->l_gt = luaH_new(L);  /* table of globals */
  init_registry(L, g);
  luaS_resize(L, MINSTRTABSIZE);  /* initial size of string table */
  luaT_init(L);
  luaX_init(L);
  luaS_fix(luaS_newliteral(L, MEMERRMSG));
  g->GCthreshold = 4*g->totalbytes;
}


/*
** preinitialize a state with consistent values without allocating
** any memory (to avoid errors)
*/
static void preinit_state (lua_State *L, global_State *g) {
  G(L) = g;
  L->stack = NULL;
  L->stacksize = 0;
  L->errorJmp = NULL;
  L->hook = NULL;
  L->hookmask = 0;
  L->basehookcount = 0;
  L->allowhook = 1;
  resethookcount(L);
  L->openupval = NULL;
  L->nny = 1;
  L->status = LUA_OK;
  L->base_ci.next = L->base_ci.previous = NULL;
  L->ci = &L->base_ci;
  L->errfunc = 0;
}


static void close_state (lua_State *L) {
  global_State *g = G(L);
  luaF_close(L, L->stack);  /* close all upvalues for this thread */
  luaC_freeallobjects(L);  /* collect all objects */
  luaM_freearray(L, G(L)->strt.hash, G(L)->strt.size);
  luaZ_freebuffer(L, &g->buff);
  freestack(L);
  lua_assert(g->totalbytes == sizeof(LG));
  (*g->frealloc)(g->ud, fromstate(L), sizeof(LG), 0);
}


LUA_API lua_State *lua_newthread (lua_State *L) {
  lua_State *L1;
  lua_lock(L);
  luaC_checkGC(L);
  L1 = &luaC_newobj(L, LUA_TTHREAD, sizeof(LX), NULL, offsetof(LX, l))->th;
  setthvalue(L, L->top, L1);
  api_incr_top(L);
  preinit_state(L1, G(L));
  stack_init(L1, L);  /* init stack */
  L1->hookmask = L->hookmask;
  L1->basehookcount = L->basehookcount;
  L1->hook = L->hook;
  resethookcount(L1);
  lua_assert(iswhite(obj2gco(L1)));
  lua_unlock(L);
  luai_userstatethread(L, L1);
  return L1;
}


void luaE_freethread (lua_State *L, lua_State *L1) {
  LX *l = fromstate(L1);
  luaF_close(L1, L1->stack);  /* close all upvalues for this thread */
  lua_assert(L1->openupval == NULL);
  luai_userstatefree(L1);
  freestack(L1);
  luaM_free(L, l);
}


LUA_API lua_State *lua_newstate (lua_Alloc f, void *ud) {
  int i;
  lua_State *L;
  global_State *g;
  LG *l = cast(LG *, (*f)(ud, NULL, 0, sizeof(LG)));
  if (l == NULL) return NULL;
  L = &l->l.l;
  g = &l->g;
  L->next = NULL;
  L->tt = LUA_TTHREAD;
  g->currentwhite = bit2mask(WHITE0BIT, FIXEDBIT);
  L->marked = luaC_white(g);
  g->gckind = KGC_NORMAL;
  g->nCcalls = 0;
  set2bits(L->marked, FIXEDBIT, SFIXEDBIT);
  preinit_state(L, g);
  g->frealloc = f;
  g->ud = ud;
  g->mainthread = L;
  g->uvhead.u.l.prev = &g->uvhead;
  g->uvhead.u.l.next = &g->uvhead;
  g->GCthreshold = MAX_LUMEM;  /* no GC while building state */
  g->strt.size = 0;
  g->strt.nuse = 0;
  g->strt.hash = NULL;
  setnilvalue(&g->l_registry);
  g->l_gt = NULL;
  luaZ_initbuffer(L, &g->buff);
  g->panic = NULL;
  g->version = lua_version(NULL);
  g->gcstate = GCSpause;
  g->rootgc = obj2gco(L);
  g->tobefnz = NULL;
  g->totalbytes = sizeof(LG);
  g->gcpause = LUAI_GCPAUSE;
  g->gcstepmul = LUAI_GCMUL;
  for (i=0; i<NUM_TAGS; i++) g->mt[i] = NULL;
  if (luaD_rawrunprotected(L, f_luaopen, NULL) != LUA_OK) {
    /* memory allocation error: free partial state */
    close_state(L);
    L = NULL;
  }
  else
    luai_userstateopen(L);
  return L;
}


LUA_API void lua_close (lua_State *L) {
  L = G(L)->mainthread;  /* only the main thread can be closed */
  lua_lock(L);
  luaF_close(L, L->stack);  /* close all upvalues for this thread */
  luaC_separateudata(L, 1);  /* separate all udata with GC metamethods */
  lua_assert(L->next == NULL);
  luai_userstateclose(L);
  close_state(L);
}


