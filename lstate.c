/*
** $Id: lstate.c,v 1.17 1999/11/22 13:12:07 roberto Exp roberto $
** Global State
** See Copyright Notice in lua.h
*/


#define LUA_REENTRANT

#include "lbuiltin.h"
#include "ldo.h"
#include "lgc.h"
#include "llex.h"
#include "lmem.h"
#include "lref.h"
#include "lstate.h"
#include "lstring.h"
#include "ltm.h"


lua_State *lua_state = NULL;


lua_State *lua_newstate (void) {
  lua_State *L = luaM_new(NULL, lua_State);
  L->Cstack.base = 0;
  L->Cstack.lua2C = 0;
  L->Cstack.num = 0;
  L->errorJmp = NULL;
  L->Mbuffer = NULL;
  L->Mbuffbase = 0;
  L->Mbuffsize = 0;
  L->Mbuffnext = 0;
  L->Cblocks = NULL;
  L->numCblocks = 0;
  L->debug = 0;
  L->callhook = NULL;
  L->linehook = NULL;
  L->rootproto = NULL;
  L->rootcl = NULL;
  L->rootglobal = NULL;
  L->roottable = NULL;
  L->IMtable = NULL;
  L->refArray = NULL;
  L->refSize = 0;
  L->refFree = NONEXT;
  L->nblocks = 0;
  L->GCthreshold = MAX_INT;  /* to avoid GC during pre-definitions */
  luaD_init(L);
  luaS_init(L);
  luaX_init(L);
  luaT_init(L);
  luaB_predefine(L);
  L->GCthreshold = L->nblocks*4;
  return L;
}


void lua_close (lua_State *L) {
  luaC_collect(L, 1);  /* collect all elements */
  LUA_ASSERT(L, L->rootproto == NULL, "list should be empty");
  LUA_ASSERT(L, L->rootcl == NULL, "list should be empty");
  LUA_ASSERT(L, L->rootglobal == NULL, "list should be empty");
  LUA_ASSERT(L, L->roottable == NULL, "list should be empty");
  luaS_freeall(L);
  luaM_free(L, L->stack.stack);
  luaM_free(L, L->IMtable);
  luaM_free(L, L->refArray);
  luaM_free(L, L->Mbuffer);
  luaM_free(L, L->Cblocks);
  LUA_ASSERT(L, L->nblocks == 0, "wrong count for nblocks");
  luaM_free(L, L);
  LUA_ASSERT(L, numblocks == 0, "memory leak!");
  LUA_ASSERT(L, totalmem == 0,"memory leak!");
  L = NULL;
}

