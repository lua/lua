/*
** $Id: lstate.c,v 1.110 2002/11/13 11:31:39 roberto Exp roberto $
** Global State
** See Copyright Notice in lua.h
*/


#include "lua.h"

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


/*
** macro to allow the inclusion of user information in Lua state
*/
#ifndef LUA_USERSTATE
#define EXTRASPACE	0
#else
union UEXTRASPACE {L_Umaxalign a; LUA_USERSTATE b;};
#define EXTRASPACE (sizeof(union UEXTRASPACE))
#endif


static void close_state (lua_State *L);


static lua_State *newthread (lua_State *L) {
  lu_byte *block = (lu_byte *)luaM_malloc(L, sizeof(lua_State) + EXTRASPACE);
  if (block == NULL) return NULL;
  else {
    block += EXTRASPACE;
    return cast(lua_State *, block);
  }
}


/*
** you can change this function through the official API:
** call `lua_setpanicf'
*/
static int default_panic (lua_State *L) {
  UNUSED(L);
  return 0;
}


static void stack_init (lua_State *L1, lua_State *L) {
  L1->stack = luaM_newvector(L, BASIC_STACK_SIZE + EXTRA_STACK, TObject);
  L1->stacksize = BASIC_STACK_SIZE + EXTRA_STACK;
  L1->top = L1->stack;
  L1->stack_last = L1->stack+(L1->stacksize - EXTRA_STACK)-1;
  L1->base_ci = luaM_newvector(L, BASIC_CI_SIZE, CallInfo);
  L1->ci = L1->base_ci;
  L1->ci->state = CI_C;  /*  not a Lua function */
  setnilvalue(L1->top++);  /* `function' entry for this `ci' */
  L1->ci->base = L1->top;
  L1->ci->top = L1->top + LUA_MINSTACK;
  L1->size_ci = BASIC_CI_SIZE;
  L1->end_ci = L1->base_ci + L1->size_ci;
}


/*
** open parts that may cause memory-allocation errors
*/
static void f_luaopen (lua_State *L, void *ud) {
  UNUSED(ud);
  /* create a new global state */
  L->l_G = luaM_new(L, global_State);
  G(L)->mainthread = L;
  G(L)->GCthreshold = 0;  /* mark it as unfinished state */
  G(L)->strt.size = 0;
  G(L)->strt.nuse = 0;
  G(L)->strt.hash = NULL;
  setnilvalue(defaultmeta(L));
  setnilvalue(registry(L));
  luaZ_initbuffer(L, &G(L)->buff);
  G(L)->panic = &default_panic;
  G(L)->rootgc = NULL;
  G(L)->rootudata = NULL;
  G(L)->tmudata = NULL;
  setnilvalue(key(G(L)->dummynode));
  setnilvalue(val(G(L)->dummynode));
  G(L)->dummynode->next = NULL;
  G(L)->nblocks = sizeof(lua_State) + sizeof(global_State);
  stack_init(L, L);  /* init stack */
  /* create default meta table with a dummy table, and then close the loop */
  defaultmeta(L)->tt = LUA_TTABLE;
  sethvalue(defaultmeta(L), luaH_new(L, 0, 4));
  hvalue(defaultmeta(L))->metatable = hvalue(defaultmeta(L));
  sethvalue(gt(L), luaH_new(L, 0, 4));  /* table of globals */
  sethvalue(registry(L), luaH_new(L, 0, 0));  /* registry */
  luaS_resize(L, MINSTRTABSIZE);  /* initial size of string table */
  luaT_init(L);
  luaX_init(L);
  luaS_fix(luaS_newliteral(L, MEMERRMSG));
  G(L)->GCthreshold = 4*G(L)->nblocks;
}


static void preinit_state (lua_State *L) {
  L->stack = NULL;
  L->stacksize = 0;
  L->errorJmp = NULL;
  L->hook = NULL;
  L->hookmask = 0;
  setallowhook(L, 1);
  resethookcount(L);
  L->openupval = NULL;
  L->size_ci = 0;
  L->base_ci = L->ci = NULL;
  L->errfunc = 0;
  setnilvalue(gt(L));
}


lua_State *luaE_newthread (lua_State *L) {
  lua_State *L1 = newthread(L);
  luaC_link(L, valtogco(L1), LUA_TTHREAD);
  preinit_state(L1);
  L1->l_G = L->l_G;
  stack_init(L1, L);  /* init stack */
  setobj2n(gt(L1), gt(L));  /* share table of globals */
  return L1;
}


LUA_API lua_State *lua_open (void) {
  lua_State *L = newthread(NULL);
  if (L) {  /* allocation OK? */
    L->tt = LUA_TTHREAD;
    preinit_state(L);
    L->l_G = NULL;
    if (luaD_rawrunprotected(L, f_luaopen, NULL) != 0) {
      /* memory allocation error: free partial state */
      close_state(L);
      L = NULL;
    }
  }
  lua_userstateopen(L);
  return L;
}


void luaE_freethread (lua_State *L, lua_State *L1) {
  luaF_close(L1, L1->stack);  /* close all upvalues for this thread */
  lua_assert(L1->openupval == NULL);
  luaM_freearray(L, L1->base_ci, L1->size_ci, CallInfo);
  luaM_freearray(L, L1->stack, L1->stacksize, TObject);
  luaM_free(L, cast(lu_byte *, L1) - EXTRASPACE,
                sizeof(lua_State) + EXTRASPACE);
}


static void close_state (lua_State *L) {
  luaF_close(L, L->stack);  /* close all upvalues for this thread */
  if (G(L)) {  /* close global state */
    luaC_sweep(L, 1);  /* collect all elements */
    lua_assert(G(L)->rootgc == NULL);
    lua_assert(G(L)->rootudata == NULL);
    luaS_freeall(L);
    luaZ_freebuffer(L, &G(L)->buff);
    luaM_freelem(NULL, L->l_G);
  }
  luaE_freethread(NULL, L);
}


LUA_API void lua_close (lua_State *L) {
  lua_lock(L);
  L = G(L)->mainthread;  /* only the main thread can be closed */
  luaC_callallgcTM(L);  /* call GC tag methods for all udata */
  lua_assert(G(L)->tmudata == NULL);
  close_state(L);
}

