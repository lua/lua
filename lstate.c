/*
** $Id: lstate.c,v 1.35 2000/08/31 13:30:39 roberto Exp roberto $
** Global State
** See Copyright Notice in lua.h
*/


#include <stdarg.h>

#include "lua.h"

#include "ldo.h"
#include "lgc.h"
#include "llex.h"
#include "lmem.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"


#ifdef DEBUG
extern lua_State *lua_state;
void luaB_opentests (lua_State *L);
#endif


lua_State *lua_newstate (int stacksize) {
  struct lua_longjmp myErrorJmp;
  lua_State *L = luaM_new(NULL, lua_State);
  if (L == NULL) return NULL;  /* memory allocation error */
  L->stack = NULL;
  L->strt.size = L->udt.size = 0;
  L->strt.nuse = L->udt.nuse = 0;
  L->strt.hash = NULL;
  L->udt.hash = NULL;
  L->Mbuffer = NULL;
  L->Mbuffbase = 0;
  L->Mbuffsize = 0;
  L->Mbuffnext = 0;
  L->rootproto = NULL;
  L->rootcl = NULL;
  L->roottable = NULL;
  L->IMtable = NULL;
  L->last_tag = -1;
  L->refArray = NULL;
  L->refSize = 0;
  L->refFree = NONEXT;
  L->nblocks = 0;
  L->GCthreshold = MAX_INT;  /* to avoid GC during pre-definitions */
  L->callhook = NULL;
  L->linehook = NULL;
  L->allowhooks = 1;
  L->errorJmp = &myErrorJmp;
  if (setjmp(myErrorJmp.b) == 0) {  /* to catch memory allocation errors */
    L->gt = luaH_new(L, 10);
    luaD_init(L, (stacksize == 0) ? DEFAULT_STACK_SIZE :
                                    stacksize+LUA_MINSTACK);
    luaS_init(L);
    luaX_init(L);
    luaT_init(L);
#ifdef DEBUG
    luaB_opentests(L);
#endif
    L->GCthreshold = L->nblocks*4;
    L->errorJmp = NULL;
    return L;
  }
  else {  /* memory allocation error: free partial state */
    lua_close(L);
    return NULL;
  }
}


void lua_close (lua_State *L) {
  luaC_collect(L, 1);  /* collect all elements */
  LUA_ASSERT(L->rootproto == NULL, "list should be empty");
  LUA_ASSERT(L->rootcl == NULL, "list should be empty");
  LUA_ASSERT(L->roottable == NULL, "list should be empty");
  luaS_freeall(L);
  luaM_free(L, L->stack);
  luaM_free(L, L->IMtable);
  luaM_free(L, L->refArray);
  luaM_free(L, L->Mbuffer);
  LUA_ASSERT(L->nblocks == 0, "wrong count for nblocks");
  luaM_free(L, L);
  LUA_ASSERT(L->Cbase == L->stack, "stack not empty");
  LUA_ASSERT(L != lua_state || memdebug_numblocks == 0, "memory leak!");
  LUA_ASSERT(L != lua_state || memdebug_total == 0,"memory leak!");
}

