/*
** $Id: lstate.c,v 1.1 2001/11/29 22:14:34 rieru Exp rieru $
** Global State
** See Copyright Notice in lua.h
*/


#include <stdio.h>

#include "lua.h"

#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "llex.h"
#include "lmem.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"


struct Sopen {
  int stacksize;
  lua_State *L;
};


static void close_state (lua_State *L, lua_State *OL);


/*
** open parts that may cause memory-allocation errors
*/
static void f_luaopen (lua_State *L, void *ud) {
  struct Sopen *so = cast(struct Sopen *, ud);
  if (so->stacksize == 0)
    so->stacksize = DEFAULT_STACK_SIZE;
  else
    so->stacksize += LUA_MINSTACK;
  if (so->L != NULL) {  /* shared global state? */
    L->_G = G(so->L);
    so->L->next->previous = L;  /* insert L into linked list */
    L->next = so->L->next;
    so->L->next = L;
    L->previous = so->L;
    luaD_init(L, so->stacksize);  /* init stack */
    setobj(defaultet(L), defaultet(so->L));  /* share default event table */
    setobj(gt(L), gt(so->L));  /* share table of globals */
    setobj(registry(L), registry(so->L));  /* share registry */
  }
  else {  /* create a new global state */
    L->_G = luaM_new(L, global_State);
    G(L)->strt.size = 0;
    G(L)->strt.nuse = 0;
    G(L)->strt.hash = NULL;
    G(L)->Mbuffer = NULL;
    G(L)->Mbuffsize = 0;
    G(L)->rootproto = NULL;
    G(L)->rootcl = NULL;
    G(L)->roottable = NULL;
    G(L)->rootupval = NULL;
    G(L)->rootudata = NULL;
    G(L)->tmudata = NULL;
    G(L)->nblocks = sizeof(lua_State) + sizeof(global_State);
    luaD_init(L, so->stacksize);  /* init stack */
    /* create default event table with a dummy table, and then close the loop */
    sethvalue(defaultet(L), NULL);
    sethvalue(defaultet(L), luaH_new(L, 0, 4));
    hvalue(defaultet(L))->eventtable = hvalue(defaultet(L));
    sethvalue(gt(L), luaH_new(L, 0, 4));  /* table of globals */
    sethvalue(registry(L), luaH_new(L, 0, 0));  /* registry */
    luaS_resize(L, 4);  /* initial size of string table */
    luaT_init(L);
    luaX_init(L);
    G(L)->GCthreshold = 4*G(L)->nblocks;
  }
}


LUA_API lua_State *lua_newthread (lua_State *OL, int stacksize) {
  struct Sopen so;
  lua_State *L;
  if (OL) lua_lock(OL);
  L = luaM_new(OL, lua_State);
  if (L) {  /* allocation OK? */
    L->_G = NULL;
    L->stack = NULL;
    L->stacksize = 0;
    L->errorJmp = NULL;
    L->callhook = NULL;
    L->linehook = NULL;
    L->openupval = NULL;
    L->size_ci = 0;
    L->base_ci = NULL;
    L->allowhooks = 1;
    L->next = L->previous = L;
    so.stacksize = stacksize;
    so.L = OL;
    if (luaD_runprotected(L, f_luaopen, &so) != 0) {
      /* memory allocation error: free partial state */
      close_state(L, OL);
      L = NULL;
    }
  }
  if (OL) lua_unlock(OL);
  return L;
}


static void close_state (lua_State *L, lua_State *OL) {
  luaF_close(L, L->stack);  /* close all upvalues for this thread */
  lua_assert(L->openupval == NULL);
  if (OL != NULL) {  /* are there other threads? */
    lua_assert(L->previous != L);
    L->previous->next = L->next;
    L->next->previous = L->previous;
  }
  else if (G(L)) {  /* last thread; close global state */
    if (G(L)->rootudata)  /* (avoid problems with incomplete states) */
      luaC_callallgcTM(L);  /* call GC tag methods for all udata */
    lua_assert(G(L)->tmudata == NULL);
    luaC_collect(L, 1);  /* collect all elements */
    lua_assert(G(L)->rootproto == NULL);
    lua_assert(G(L)->rootudata == NULL);
    lua_assert(G(L)->rootcl == NULL);
    lua_assert(G(L)->rootupval == NULL);
    lua_assert(G(L)->roottable == NULL);
    luaS_freeall(L);
    luaM_freearray(L, G(L)->Mbuffer, G(L)->Mbuffsize, char);
    luaM_freelem(NULL, L->_G);
  }
  luaM_freearray(OL, L->base_ci, L->size_ci, CallInfo);
  luaM_freearray(OL, L->stack, L->stacksize, TObject);
  luaM_freelem(OL, L);
}


LUA_API void lua_close (lua_State *L) {
  lua_State *OL;
  lua_assert(L != lua_state || lua_gettop(L) == 0);
  lua_lock(L);
  OL = L->next;  /* any surviving thread (if there is one) */
  if (OL == L) OL = NULL;  /* no surviving threads */
  close_state(L, OL);
  if (OL) lua_unlock(OL);  /* cannot unlock over a freed state */
  lua_assert(L != lua_state || memdebug_numblocks == 0);
  lua_assert(L != lua_state || memdebug_total == 0);
}

