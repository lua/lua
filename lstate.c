/*
** $Id: lstate.c,v 1.100 2002/08/05 17:36:24 roberto Exp roberto $
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




static void close_state (lua_State *L);


/*
** you can change this function through the official API
** call `lua_setpanicf'
*/
static int default_panic (lua_State *L) {
  UNUSED(L);
  return 0;
}


static void stack_init (lua_State *L, lua_State *OL) {
  L->stack = luaM_newvector(OL, BASIC_STACK_SIZE + EXTRA_STACK, TObject);
  L->stacksize = BASIC_STACK_SIZE + EXTRA_STACK;
  L->top = L->stack;
  L->stack_last = L->stack+(L->stacksize - EXTRA_STACK)-1;
  L->base_ci = luaM_newvector(OL, BASIC_CI_SIZE, CallInfo);
  L->ci = L->base_ci;
  L->ci->pc = NULL;  /*  not a Lua function */
  setnilvalue(L->top++);  /* `function' entry for this `ci' */
  L->ci->base = L->top;
  L->ci->top = L->top + LUA_MINSTACK;
  L->size_ci = BASIC_CI_SIZE;
  L->end_ci = L->base_ci + L->size_ci;
  L->toreset = luaM_newvector(OL, 2, Protection);
  L->size_toreset = 2;
}


/*
** open parts that may cause memory-allocation errors
*/
static void f_luaopen (lua_State *L, void *ud) {
  UNUSED(ud);
  /* create a new global state */
  L->l_G = luaM_new(L, global_State);
  G(L)->GCthreshold = 0;  /* mark it as unfinished state */
  G(L)->strt.size = 0;
  G(L)->strt.nuse = 0;
  G(L)->strt.hash = NULL;
  G(L)->Mbuffer = NULL;
  G(L)->Mbuffsize = 0;
  G(L)->panic = &default_panic;
  G(L)->rootproto = NULL;
  G(L)->rootcl = NULL;
  G(L)->roottable = NULL;
  G(L)->rootupval = NULL;
  G(L)->rootudata = NULL;
  G(L)->tmudata = NULL;
  setnilvalue(key(G(L)->dummynode));
  setnilvalue(val(G(L)->dummynode));
  G(L)->dummynode->next = NULL;
  G(L)->nblocks = sizeof(lua_State) + sizeof(global_State);
  stack_init(L, L);  /* init stack */
  /* create default meta table with a dummy table, and then close the loop */
  sethvalue(defaultmeta(L), NULL);
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
  L->toreset = NULL;
  L->size_toreset = L->number_toreset = 0;
  setnilvalue(defaultmeta(L));
  setnilvalue(gt(L));
  setnilvalue(registry(L));
}


LUA_API lua_State *lua_newthread (lua_State *OL) {
  lua_State *L;
  lua_lock(OL);
  L = luaM_new(OL, lua_State);
  preinit_state(L);
  L->l_G = OL->l_G;
  OL->next->previous = L;  /* insert L into linked list */
  L->next = OL->next;
  OL->next = L;
  L->previous = OL;
  stack_init(L, OL);  /* init stack */
  setobj(defaultmeta(L), defaultmeta(OL));  /* share default meta table */
  setobj(gt(L), gt(OL));  /* share table of globals */
  setobj(registry(L), registry(OL));  /* share registry */
  lua_unlock(OL);
  lua_userstateopen(L);
  return L;
}


LUA_API lua_State *lua_open (void) {
  lua_State *L;
  L = luaM_new(NULL, lua_State);
  if (L) {  /* allocation OK? */
    preinit_state(L);
    L->l_G = NULL;
    L->next = L->previous = L;
    if (luaD_rawrunprotected(L, f_luaopen, NULL) != 0) {
      /* memory allocation error: free partial state */
      close_state(L);
      L = NULL;
    }
  }
  lua_userstateopen(L);
  return L;
}


void luaE_closethread (lua_State *OL, lua_State *L) {
  luaF_close(L, L->stack);  /* close all upvalues for this thread */
  lua_assert(L->openupval == NULL);
  L->previous->next = L->next;
  L->next->previous = L->previous;
  luaM_freearray(OL, L->base_ci, L->size_ci, CallInfo);
  luaM_freearray(OL, L->toreset, L->size_toreset, Protection);
  luaM_freearray(OL, L->stack, L->stacksize, TObject);
  luaM_freelem(OL, L);
}


static void close_state (lua_State *L) {
  luaF_close(L, L->stack);  /* close all upvalues for this thread */
  if (G(L)) {  /* close global state */
    luaC_collect(L, 1);  /* collect all elements */
    lua_assert(G(L)->rootproto == NULL);
    lua_assert(G(L)->rootudata == NULL);
    lua_assert(G(L)->rootcl == NULL);
    lua_assert(G(L)->rootupval == NULL);
    lua_assert(G(L)->roottable == NULL);
    luaS_freeall(L);
    luaM_freearray(L, G(L)->Mbuffer, G(L)->Mbuffsize, char);
    luaM_freelem(NULL, L->l_G);
  }
  luaE_closethread(NULL, L);
}


LUA_API void lua_closethread (lua_State *L, lua_State *thread) {
  lua_lock(L);
  if (L == thread) luaG_runerror(L, "cannot close only thread of a state");
  luaE_closethread(L, thread);
  lua_unlock(L);
}


LUA_API void lua_close (lua_State *L) {
  lua_lock(L);
  luaC_callallgcTM(L);  /* call GC tag methods for all udata */
  lua_assert(G(L)->tmudata == NULL);
  while (L->next != L)  /* then, close all other threads */
    luaE_closethread(L, L->next);
  close_state(L);
}

