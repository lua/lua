/*
** $Id: $
** REF mechanism
** See Copyright Notice in lua.h
*/


#include "lmem.h"
#include "lref.h"
#include "lstate.h"
#include "lua.h"


int luaR_ref (const TObject *o, int lock) {
  int ref;
  if (ttype(o) == LUA_T_NIL)
    ref = LUA_REFNIL;
  else {
    for (ref=0; ref<L->refSize; ref++)
      if (L->refArray[ref].status == FREE)
        break;
    if (ref == L->refSize) {  /* no more empty spaces? */
      luaM_growvector(L->refArray, L->refSize, 1, struct ref, refEM, MAX_INT);
      L->refSize++;
    }
    L->refArray[ref].o = *o;
    L->refArray[ref].status = lock ? LOCK : HOLD;
  }
  return ref;
}


void lua_unref (int ref) {
  if (ref >= 0 && ref < L->refSize)
    L->refArray[ref].status = FREE;
}


const TObject *luaR_getref (int ref) {
  if (ref == LUA_REFNIL)
    return &luaO_nilobject;
  if (ref >= 0 && ref < L->refSize &&
      (L->refArray[ref].status == LOCK || L->refArray[ref].status == HOLD))
    return &L->refArray[ref].o;
  else
    return NULL;
}


static int ismarked (const TObject *o) {
  /* valid only for locked objects */
  switch (o->ttype) {
    case LUA_T_STRING: case LUA_T_USERDATA:
      return o->value.ts->marked;
    case LUA_T_ARRAY:
      return o->value.a->marked;
    case LUA_T_CLOSURE:
      return o->value.cl->marked;
    case LUA_T_PROTO:
      return o->value.tf->marked;
#ifdef DEBUG
    case LUA_T_LINE: case LUA_T_CLMARK:
    case LUA_T_CMARK: case LUA_T_PMARK:
      LUA_INTERNALERROR("invalid type");
#endif
    default:  /* nil, number or cproto */
      return 1;
  }
}


void luaR_invalidaterefs (void) {
  int i;
  for (i=0; i<L->refSize; i++)
    if (L->refArray[i].status == HOLD && !ismarked(&L->refArray[i].o))
      L->refArray[i].status = COLLECTED;
}

