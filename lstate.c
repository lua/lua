/*
** $Id: lstate.c,v 1.13 1999/08/16 20:52:00 roberto Exp roberto $
** Global State
** See Copyright Notice in lua.h
*/


#include "lbuiltin.h"
#include "ldo.h"
#include "lgc.h"
#include "llex.h"
#include "lmem.h"
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
  L->GCthreshold = GARBAGE_BLOCK;
  L->nblocks = 0;
  luaD_init();
  luaS_init();
  luaX_init();
  luaT_init();
  luaB_predefine();
}


void lua_close (void) {
  luaC_collect(1);  /* collect all elements */
  luaS_freeall();
  luaM_free(L->stack.stack);
  luaM_free(L->IMtable);
  luaM_free(L->refArray);
  luaM_free(L->Mbuffer);
  luaM_free(L->Cblocks);
  LUA_ASSERT(L->nblocks == 0, "wrong count for nblocks");
  luaM_free(L);
  L = NULL;
#ifdef DEBUG
  printf("total de blocos: %ld\n", numblocks);
  printf("total de memoria: %ld\n", totalmem);
#endif
}


