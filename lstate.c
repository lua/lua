/*
** $Id: $
** Global State
** See Copyright Notice in lua.h
*/


#include "lbuiltin.h"
#include "ldo.h"
#include "llex.h"
#include "lmem.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"


LState *lua_state = NULL;


void lua_open (void)
{
  if (lua_state) return;
  lua_state = luaM_new(LState);
  L->numCblocks = 0;
  L->Cstack.base = 0;
  L->Cstack.lua2C = 0;
  L->Cstack.num = 0;
  L->errorJmp = NULL;
  L->rootproto.next = NULL;
  L->rootproto.marked = 0;
  L->rootcl.next = NULL;
  L->rootcl.marked = 0;
  L->rootglobal.next = NULL;
  L->rootglobal.marked = 0;
  L->roottable.next = NULL;
  L->roottable.marked = 0;
  L->refArray = NULL;
  L->refSize = 0;
  L->Mbuffsize = 0;
  L->Mbuffer = NULL;
  L->GCthreshold = GARBAGE_BLOCK;
  L->nblocks = 0;
  luaD_init();
  luaS_init();
  luaX_init();
  luaT_init();
  L->globalbag.ttype = LUA_T_ARRAY;
  L->globalbag.value.a = luaH_new(0);
  luaB_predefine();
}

