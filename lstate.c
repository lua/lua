/*
** $Id: lstate.c,v 1.15 1999/10/14 17:53:35 roberto Exp roberto $
** Global State
** See Copyright Notice in lua.h
*/


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


void lua_open (void) {
  if (lua_state) return;
  lua_state = luaM_new(lua_State);
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
  luaD_init();
  luaS_init();
  luaX_init();
  luaT_init();
  luaB_predefine();
  L->GCthreshold = L->nblocks*4;
}


void lua_close (void) {
  luaC_collect(1);  /* collect all elements */
  LUA_ASSERT(L->rootproto == NULL, "list should be empty");
  LUA_ASSERT(L->rootcl == NULL, "list should be empty");
  LUA_ASSERT(L->rootglobal == NULL, "list should be empty");
  LUA_ASSERT(L->roottable == NULL, "list should be empty");
  luaS_freeall();
  luaM_free(L->stack.stack);
  luaM_free(L->IMtable);
  luaM_free(L->refArray);
  luaM_free(L->Mbuffer);
  luaM_free(L->Cblocks);
  LUA_ASSERT(L->nblocks == 0, "wrong count for nblocks");
  luaM_free(L);
  LUA_ASSERT(numblocks == 0, "memory leak!");
  LUA_ASSERT(totalmem == 0,"memory leak!");
  L = NULL;
}

