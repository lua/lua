/*
** $Id: lstate.c,v 1.48 2000/10/30 16:29:59 roberto Exp $
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
#endif
  LUA_ASSERT(lua_gettop(L) == 0, "wrong API stack");
}


LUA_API lua_State *lua_open (int stacksize) {
  lua_State *L = luaM_new(NULL, lua_State);
  if (L == NULL) return NULL;  /* memory allocation error */
  L->stack = NULL;
  L->strt.size = L->udt.size = 0;
  L->strt.nuse = L->udt.nuse = 0;
  L->strt.hash = NULL;
  L->udt.hash = NULL;
  L->Mbuffer = NULL;
  L->Mbuffsize = 0;
  L->rootproto = NULL;
  L->rootcl = NULL;
  L->roottable = NULL;
  L->TMtable = NULL;
  L->last_tag = -1;
  L->refArray = NULL;
  L->refSize = 0;
  L->refFree = NONEXT;
  L->nblocks = sizeof(lua_State);
  L->GCthreshold = MAX_INT;  /* to avoid GC during pre-definitions */
  L->callhook = NULL;
  L->linehook = NULL;
  L->allowhooks = 1;
  L->errorJmp = NULL;
  if (luaD_runprotected(L, f_luaopen, &stacksize) != 0) {
    /* memory allocation error: free partial state */
    lua_close(L);
    return NULL;
  }
  L->GCthreshold = 2*L->nblocks;
  return L;
}


LUA_API void lua_close (lua_State *L) {
  LUA_ASSERT(L != lua_state || lua_gettop(L) == 0, "garbage in C stack");
  luaC_collect(L, 1);  /* collect all elements */
  LUA_ASSERT(L->rootproto == NULL, "list should be empty");
  LUA_ASSERT(L->rootcl == NULL, "list should be empty");
  LUA_ASSERT(L->roottable == NULL, "list should be empty");
  luaS_freeall(L);
  if (L->stack)
    L->nblocks -= (L->stack_last - L->stack + 1)*sizeof(TObject);
  luaM_free(L, L->stack);
  L->nblocks -= (L->last_tag+1)*sizeof(struct TM);
  luaM_free(L, L->TMtable);
  L->nblocks -= (L->refSize)*sizeof(struct Ref);
  luaM_free(L, L->refArray);
  L->nblocks -= (L->Mbuffsize)*sizeof(char);
  luaM_free(L, L->Mbuffer);
  LUA_ASSERT(L->nblocks == sizeof(lua_State), "wrong count for nblocks");
  luaM_free(L, L);
  LUA_ASSERT(L != lua_state || memdebug_numblocks == 0, "memory leak!");
  LUA_ASSERT(L != lua_state || memdebug_total == 0,"memory leak!");
}

