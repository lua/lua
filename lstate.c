/*
** $Id: lstate.c,v 1.50 2000/12/28 12:55:41 roberto Exp roberto $
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


/*
** open parts that may cause memory-allocation errors
*/
static void f_luaopen (lua_State *L, void *ud) {
  int stacksize = *(int *)ud;
  if (stacksize == 0)
    stacksize = DEFAULT_STACK_SIZE;
  else
    stacksize += LUA_MINSTACK;
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
  G(L)->GCthreshold = MAX_INT;  /* to avoid GC during pre-definitions */
  L->gt = luaH_new(L, 10);  /* table of globals */
  luaD_init(L, stacksize);
  luaS_init(L);
  luaX_init(L);
  luaT_init(L);
  lua_newtable(L);
  lua_ref(L, 1);  /* create registry */
  lua_register(L, LUA_ERRORMESSAGE, errormessage);
#ifdef LUA_DEBUG
  luaB_opentests(L);
  if (lua_state == NULL) lua_state = L;  /* keep first state to be opened */
  lua_assert(lua_gettop(L) == 0);
#endif
}


LUA_API lua_State *lua_open (int stacksize) {
  lua_State *L;
  L = luaM_new(NULL, lua_State);
  if (L == NULL) return NULL;  /* memory allocation error */
  L->G = NULL;
  L->stack = NULL;
  L->stacksize = 0;
  L->errorJmp = NULL;
  L->callhook = NULL;
  L->linehook = NULL;
  L->allowhooks = 1;
  if (luaD_runprotected(L, f_luaopen, &stacksize) != 0) {
    /* memory allocation error: free partial state */
    lua_close(L);
    return NULL;
  }
  G(L)->GCthreshold = 2*G(L)->nblocks;
  return L;
}


LUA_API void lua_close (lua_State *L) {
  lua_assert(L != lua_state || lua_gettop(L) == 0);
  if (G(L)) {  /* close global state */
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
  luaM_freearray(NULL, L->stack, L->stacksize, TObject);
  luaM_freelem(NULL, L, lua_State);
  lua_assert(L != lua_state || memdebug_numblocks == 0);
  lua_assert(L != lua_state || memdebug_total == 0);
}

