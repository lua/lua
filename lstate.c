/*
** $Id: lstate.c,v 1.52 2001/01/22 18:01:38 roberto Exp roberto $
** Global State
** See Copyright Notice in lua.h
*/


#include <stdio.h>

#include "lua.h"

#include "ldo.h"
#include "lgc.h"
#include "llex.h"
#include "lmem.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"


#ifdef LUA_DEBUG
static lua_State *lua_state = NULL;
void luaB_opentests (lua_State *L);
int islocked = 0;
#endif


/*
** built-in implementation for ERRORMESSAGE. In a "correct" environment
** ERRORMESSAGE should have an external definition, and so this function
** would not be used.
*/
static int errormessage (lua_State *L) {
  const char *s = lua_tostring(L, 1);
  if (s == NULL) s = "(no message)";
  fprintf(stderr, "error: %s\n", s);
  return 0;
}


struct Sopen {
  int stacksize;
  lua_State *L;
};


static void close_state (lua_State *L);


/*
** open parts that may cause memory-allocation errors
*/
static void f_luaopen (lua_State *L, void *ud) {
  struct Sopen *so = (struct Sopen *)ud;
  if (so->stacksize == 0)
    so->stacksize = DEFAULT_STACK_SIZE;
  else
    so->stacksize += LUA_MINSTACK;
  if (so->L != NULL) {  /* shared global state? */
    L->G = G(so->L);
    L->gt = so->L->gt;  /* share table of globals */
    so->L->next->previous = L;  /* insert L into linked list */
    L->next = so->L->next;
    so->L->next = L;
    L->previous = so->L;
    luaD_init(L, so->stacksize);  /* init stack */
  }
  else {  /* create a new global state */
    L->G = luaM_new(L, global_State);
    G(L)->strt.size = G(L)->udt.size = 0;
    G(L)->strt.nuse = G(L)->udt.nuse = 0;
    G(L)->strt.hash = G(L)->udt.hash = NULL;
    G(L)->Mbuffer = NULL;
    G(L)->Mbuffsize = 0;
    G(L)->rootproto = NULL;
    G(L)->rootcl = NULL;
    G(L)->roottable = NULL;
    G(L)->TMtable = NULL;
    G(L)->sizeTM = 0;
    G(L)->ntag = 0;
    G(L)->refArray = NULL;
    G(L)->nref = 0;
    G(L)->sizeref = 0;
    G(L)->refFree = NONEXT;
    G(L)->nblocks = sizeof(lua_State) + sizeof(global_State);
    luaD_init(L, so->stacksize);  /* init stack */
    L->gt = luaH_new(L, 10);  /* table of globals */
    luaS_init(L);
    luaX_init(L);
    luaT_init(L);
    G(L)->GCthreshold = 4*G(L)->nblocks;
    LUA_EXIT;  /* temporary exit to use the API */
    lua_newtable(L);
    lua_ref(L, 1);  /* create registry */
    lua_register(L, LUA_ERRORMESSAGE, errormessage);
#ifdef LUA_DEBUG
    luaB_opentests(L);
    if (lua_state == NULL) lua_state = L;  /* keep first state to be opened */
    lua_assert(lua_gettop(L) == 0);
#endif
    LUA_ENTRY;  /* go back inside */
  }
}


LUA_API lua_State *lua_open (lua_State *OL, int stacksize) {
  struct Sopen so;
  lua_State *L;
  LUA_ENTRY;
  L = luaM_new(OL, lua_State);
  if (L) {  /* allocation OK? */
    L->G = NULL;
    L->stack = NULL;
    L->stacksize = 0;
    L->errorJmp = NULL;
    L->callhook = NULL;
    L->linehook = NULL;
    L->allowhooks = 1;
    L->next = L->previous = L;
    so.stacksize = stacksize;
    so.L = OL;
    if (luaD_runprotected(L, f_luaopen, &so) != 0) {
      /* memory allocation error: free partial state */
      close_state(L);
      L = NULL;
    }
  }
  LUA_EXIT;
  return L;
}


static void close_state (lua_State *L) {
  lua_State *L1;
  L1 = L->next;  /* any surviving thread (if there is one) */
  if (L1 == L) L1 = NULL;  /* no surviving threads */
  if (L1 != NULL) {  /* are there other threads? */
    lua_assert(L->previous != L);
    L->previous->next = L->next;
    L->next->previous = L->previous;
  }
  else if (G(L)) {  /* last thread; close global state */
    luaC_collect(L, 1);  /* collect all elements */
    lua_assert(G(L)->rootproto == NULL);
    lua_assert(G(L)->rootcl == NULL);
    lua_assert(G(L)->roottable == NULL);
    luaS_freeall(L);
    luaM_freearray(L, G(L)->TMtable, G(L)->sizeTM, struct TM);
    luaM_freearray(L, G(L)->refArray, G(L)->sizeref, struct Ref);
    luaM_freearray(L, G(L)->Mbuffer, G(L)->Mbuffsize, char);
    luaM_freelem(NULL, L->G, global_State);
  }
  luaM_freearray(L1, L->stack, L->stacksize, TObject);
  luaM_freelem(L1, L, lua_State);
}

LUA_API void lua_close (lua_State *L) {
  lua_assert(L != lua_state || lua_gettop(L) == 0);
  LUA_ENTRY;
  close_state(L);
  LUA_EXIT;
  lua_assert(L != lua_state || memdebug_numblocks == 0);
  lua_assert(L != lua_state || memdebug_total == 0);
}

