/*
** $Id: lstate.c,v 1.28 2000/06/30 14:35:17 roberto Exp roberto $
** Global State
** See Copyright Notice in lua.h
*/


#include <stdarg.h>

#define LUA_REENTRANT

#include "lua.h"

#include "lauxlib.h"
#include "lbuiltin.h"
#include "ldo.h"
#include "lgc.h"
#include "llex.h"
#include "lmem.h"
#include "lref.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"


lua_State *lua_state = NULL;


lua_State *lua_newstate (int stacksize, int put_builtin) {
  lua_State *L = luaM_new(NULL, lua_State);
  L->errorJmp = NULL;
  L->Mbuffer = NULL;
  L->Mbuffbase = 0;
  L->Mbuffsize = 0;
  L->Mbuffnext = 0;
  L->Cblocks = NULL;
  L->numCblocks = 0;
  L->rootproto = NULL;
  L->rootcl = NULL;
  L->roottable = NULL;
  L->IMtable = NULL;
  L->refArray = NULL;
  L->refSize = 0;
  L->refFree = NONEXT;
  L->nblocks = 0;
  L->GCthreshold = MAX_INT;  /* to avoid GC during pre-definitions */
  L->debug = 0;
  L->callhook = NULL;
  L->linehook = NULL;
  L->allowhooks = 1;
  L->gt = luaH_new(L, 10);
  if (stacksize == 0) stacksize = DEFAULT_STACK_SIZE;
  luaD_init(L, stacksize);
  luaS_init(L);
  luaX_init(L);
  luaT_init(L);
  if (put_builtin)
    luaB_predefine(L);
  L->GCthreshold = L->nblocks*4;
  return L;
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
  luaM_free(L, L->Cblocks);
  LUA_ASSERT(L->numCblocks == 0, "Cblocks still open");
  LUA_ASSERT(L->nblocks == 0, "wrong count for nblocks");
  LUA_ASSERT(L->Cstack.base == L->top, "C2Lua not empty");
  luaM_free(L, L);
  if (L == lua_state) {
    LUA_ASSERT(memdebug_numblocks == 0, "memory leak!");
    LUA_ASSERT(memdebug_total == 0,"memory leak!");
    lua_state = NULL;
  }
}

