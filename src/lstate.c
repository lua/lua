/*
** $Id: lstate.c,v 1.6 1998/06/02 20:37:04 roberto Exp $
** Global State
** See Copyright Notice in lua.h
*/


#include "lbuiltin.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "llex.h"
#include "lmem.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"


lua_State *lua_state = NULL;


void lua_open (void)
{
  if (lua_state) return;
  lua_state = luaM_new(lua_State);
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
  L->Mbuffnext = 0;
  L->Mbuffbase = NULL;
  L->Mbuffer = NULL;
  L->GCthreshold = GARBAGE_BLOCK;
  L->nblocks = 0;
  luaD_init();
  luaS_init();
  luaX_init();
  luaT_init();
  luaB_predefine();
}


void lua_close (void)
{
  TaggedString *alludata = luaS_collectudata();
  L->GCthreshold = MAX_INT;  /* to avoid GC during GC */
  luaC_hashcallIM((Hash *)L->roottable.next);  /* GC t.methods for tables */
  luaC_strcallIM(alludata);  /* GC tag methods for userdata */
  luaD_gcIM(&luaO_nilobject);  /* GC tag method for nil (signal end of GC) */
  luaH_free((Hash *)L->roottable.next);
  luaF_freeproto((TProtoFunc *)L->rootproto.next);
  luaF_freeclosure((Closure *)L->rootcl.next);
  luaS_free(alludata);
  luaS_freeall();
  luaM_free(L->stack.stack);
  luaM_free(L->IMtable);
  luaM_free(L->refArray);
  luaM_free(L->Mbuffer);
  luaM_free(L);
  L = NULL;
#ifdef DEBUG
  printf("total de blocos: %ld\n", numblocks);
  printf("total de memoria: %ld\n", totalmem);
#endif
}


lua_State *lua_setstate (lua_State *st) {
  lua_State *old = lua_state;
  lua_state = st;
  return old;
}

