/*
** $Id: lstate.c,v 1.71 2001/10/31 19:58:11 roberto Exp $
** Global State
** See Copyright Notice in lua.h
*/


#include <stdio.h>

#define LUA_PRIVATE
#include "lua.h"

#include "ldo.h"
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
    L->gt = so->L->gt;  /* share table of globals */
    so->L->next->previous = L;  /* insert L into linked list */
    L->next = so->L->next;
    so->L->next = L;
    L->previous = so->L;
    luaD_init(L, so->stacksize);  /* init stack */
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
    G(L)->rootudata = NULL;
    G(L)->rootupval = NULL;
    G(L)->TMtable = NULL;
    G(L)->sizeTM = 0;
    G(L)->ntag = 0;
    G(L)->nblocks = sizeof(lua_State) + sizeof(global_State);
    luaD_init(L, so->stacksize);  /* init stack */
    sethvalue(&L->gt, luaH_new(L, 0, 4));  /* table of globals */
    G(L)->type2tag = luaH_new(L, 0, 3);
    sethvalue(&G(L)->registry, luaH_new(L, 0, 0));
    luaS_resize(L, 4);  /* initial size of string table */
    luaX_init(L);
    luaT_init(L);
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
    L->ci = &L->basefunc;
    L->basefunc.prev = NULL;
    L->errorJmp = NULL;
    L->callhook = NULL;
    L->linehook = NULL;
    L->opencl = NULL;
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
  if (OL != NULL) {  /* are there other threads? */
    lua_assert(L->previous != L);
    L->previous->next = L->next;
    L->next->previous = L->previous;
  }
  else if (G(L)) {  /* last thread; close global state */
    luaC_callallgcTM(L);  /* call GC tag methods for all udata */
    luaC_collect(L, 1);  /* collect all elements */
    lua_assert(G(L)->rootproto == NULL);
    lua_assert(G(L)->rootudata == NULL);
    lua_assert(G(L)->rootcl == NULL);
    lua_assert(G(L)->roottable == NULL);
    luaS_freeall(L);
    luaM_freearray(L, G(L)->TMtable, G(L)->sizeTM, struct TM);
    luaM_freearray(L, G(L)->Mbuffer, G(L)->Mbuffsize, l_char);
    luaM_freelem(NULL, L->_G);
  }
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

