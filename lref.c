/*
** $Id: lref.c,v 1.17 2000/08/09 19:16:57 roberto Exp roberto $
** reference mechanism
** See Copyright Notice in lua.h
*/


#include "lua.h"

#include "lapi.h"
#include "ldo.h"
#include "lmem.h"
#include "lref.h"
#include "lstate.h"


int lua_ref (lua_State *L,  int lock) {
  int ref;
  luaA_checkCargs(L, 1);
  if (ttype(L->top-1) == TAG_NIL)
    ref = LUA_REFNIL;
  else {
    if (L->refFree != NONEXT) {  /* is there a free place? */
      ref = L->refFree;
      L->refFree = L->refArray[ref].st;
    }
    else {  /* no more free places */
      luaM_growvector(L, L->refArray, L->refSize, 1, struct Ref,
                      "reference table overflow", MAX_INT);
      ref = L->refSize++;
    }
    L->refArray[ref].o = *(L->top-1);
    L->refArray[ref].st = lock ? LOCK : HOLD;
  }
  L->top--;
  return ref;
}


void lua_unref (lua_State *L, int ref) {
  if (ref >= 0) {
    if (ref >= L->refSize || L->refArray[ref].st >= 0)
      lua_error(L, "Lua API error - "
                   "invalid argument for function `lua_unref'");
    L->refArray[ref].st = L->refFree;
    L->refFree = ref;
  }
}


int lua_pushref (lua_State *L, int ref) {
  if (ref == LUA_REFNIL)
    ttype(L->top) = TAG_NIL;
  else if (0 <= ref && ref < L->refSize &&
          (L->refArray[ref].st == LOCK || L->refArray[ref].st == HOLD))
    *L->top = L->refArray[ref].o;
  else
    return 0;
  incr_top;
  return 1;
}


void lua_beginblock (lua_State *L) {
  luaM_growvector(L, L->Cblocks, L->numCblocks, 1, struct C_Lua_Stack,
                  "too many nested blocks", L->stacksize);
  L->Cblocks[L->numCblocks] = L->Cstack;
  L->numCblocks++;
}


void lua_endblock (lua_State *L) {
  if (L->numCblocks <= 0)
    lua_error(L, "Lua API error - no block to end");
  --L->numCblocks;
  L->Cstack = L->Cblocks[L->numCblocks];
  L->top = L->Cstack.base;
}






static int hasmark (const TObject *o) {
  /* valid only for locked objects */
  switch (o->ttype) {
    case TAG_STRING: case TAG_USERDATA:
      return tsvalue(o)->marked;
    case TAG_TABLE:
      return ismarked(hvalue(o));
    case TAG_LCLOSURE:  case TAG_CCLOSURE:
      return ismarked(clvalue(o)->mark);
    default:  /* number */
      return 1;
  }
}


/* for internal debugging only; check if a link of free refs is valid */
#define VALIDLINK(L, st,n)	(NONEXT <= (st) && (st) < (n))

void luaR_invalidaterefs (lua_State *L) {
  int n = L->refSize;
  int i;
  for (i=0; i<n; i++) {
    struct Ref *r = &L->refArray[i];
    if (r->st == HOLD && !hasmark(&r->o))
      r->st = COLLECTED;
    LUA_ASSERT((r->st == LOCK && hasmark(&r->o)) ||
                r->st == COLLECTED ||
                r->st == NONEXT ||
               (r->st < n && VALIDLINK(L, L->refArray[r->st].st, n)),
               "inconsistent ref table");
  }
  LUA_ASSERT(VALIDLINK(L, L->refFree, n), "inconsistent ref table");
}

