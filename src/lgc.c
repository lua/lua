/*
** $Id: lgc.c,v 1.23 1999/03/04 21:17:26 roberto Exp $
** Garbage Collector
** See Copyright Notice in lua.h
*/


#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lua.h"



static int markobject (TObject *o);



/*
** =======================================================
** REF mechanism
** =======================================================
*/


int luaC_ref (TObject *o, int lock) {
  int ref;
  if (ttype(o) == LUA_T_NIL)
    ref = -1;   /* special ref for nil */
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


void lua_unref (int ref)
{
  if (ref >= 0 && ref < L->refSize)
    L->refArray[ref].status = FREE;
}


TObject* luaC_getref (int ref)
{
  if (ref == -1)
    return &luaO_nilobject;
  if (ref >= 0 && ref < L->refSize &&
      (L->refArray[ref].status == LOCK || L->refArray[ref].status == HOLD))
    return &L->refArray[ref].o;
  else
    return NULL;
}


static void travlock (void)
{
  int i;
  for (i=0; i<L->refSize; i++)
    if (L->refArray[i].status == LOCK)
      markobject(&L->refArray[i].o);
}


static int ismarked (TObject *o)
{
  /* valid only for locked objects */
  switch (o->ttype) {
    case LUA_T_STRING: case LUA_T_USERDATA:
      return o->value.ts->head.marked;
    case LUA_T_ARRAY:
      return o->value.a->head.marked;
    case LUA_T_CLOSURE:
      return o->value.cl->head.marked;
    case LUA_T_PROTO:
      return o->value.tf->head.marked;
#ifdef DEBUG
    case LUA_T_LINE: case LUA_T_CLMARK:
    case LUA_T_CMARK: case LUA_T_PMARK:
      LUA_INTERNALERROR("invalid type");
#endif
    default:  /* nil, number or cproto */
      return 1;
  }
}


static void invalidaterefs (void)
{
  int i;
  for (i=0; i<L->refSize; i++)
    if (L->refArray[i].status == HOLD && !ismarked(&L->refArray[i].o))
      L->refArray[i].status = COLLECTED;
}



void luaC_hashcallIM (Hash *l)
{
  TObject t;
  ttype(&t) = LUA_T_ARRAY;
  for (; l; l=(Hash *)l->head.next) {
    avalue(&t) = l;
    luaD_gcIM(&t);
  }
}


void luaC_strcallIM (TaggedString *l)
{
  TObject o;
  ttype(&o) = LUA_T_USERDATA;
  for (; l; l=(TaggedString *)l->head.next)
    if (l->constindex == -1) {  /* is userdata? */
      tsvalue(&o) = l;
      luaD_gcIM(&o);
    }
}



static GCnode *listcollect (GCnode *l)
{
  GCnode *frees = NULL;
  while (l) {
    GCnode *next = l->next;
    l->marked = 0;
    while (next && !next->marked) {
      l->next = next->next;
      next->next = frees;
      frees = next;
      next = l->next;
    }
    l = next;
  }
  return frees;
}


static void strmark (TaggedString *s)
{
  if (!s->head.marked)
    s->head.marked = 1;
}


static void protomark (TProtoFunc *f) {
  if (!f->head.marked) {
    int i;
    f->head.marked = 1;
    strmark(f->source);
    for (i=0; i<f->nconsts; i++)
      markobject(&f->consts[i]);
  }
}


static void closuremark (Closure *f)
{
  if (!f->head.marked) {
    int i;
    f->head.marked = 1;
    for (i=f->nelems; i>=0; i--)
      markobject(&f->consts[i]);
  }
}


static void hashmark (Hash *h)
{
  if (!h->head.marked) {
    int i;
    h->head.marked = 1;
    for (i=0; i<nhash(h); i++) {
      Node *n = node(h,i);
      if (ttype(ref(n)) != LUA_T_NIL) {
        markobject(&n->ref);
        markobject(&n->val);
      }
    }
  }
}


static void globalmark (void)
{
  TaggedString *g;
  for (g=(TaggedString *)L->rootglobal.next; g; g=(TaggedString *)g->head.next){
    LUA_ASSERT(g->constindex >= 0, "userdata in global list");
    if (g->u.s.globalval.ttype != LUA_T_NIL) {
      markobject(&g->u.s.globalval);
      strmark(g);  /* cannot collect non nil global variables */
    }
  }
}


static int markobject (TObject *o)
{
  switch (ttype(o)) {
    case LUA_T_USERDATA:  case LUA_T_STRING:
      strmark(tsvalue(o));
      break;
    case LUA_T_ARRAY:
      hashmark(avalue(o));
      break;
    case LUA_T_CLOSURE:  case LUA_T_CLMARK:
      closuremark(o->value.cl);
      break;
    case LUA_T_PROTO: case LUA_T_PMARK:
      protomark(o->value.tf);
      break;
    default: break;  /* numbers, cprotos, etc */
  }
  return 0;
}



static void markall (void)
{
  luaD_travstack(markobject); /* mark stack objects */
  globalmark();  /* mark global variable values and names */
  travlock(); /* mark locked objects */
  luaT_travtagmethods(markobject);  /* mark fallbacks */
}


long lua_collectgarbage (long limit)
{
  unsigned long recovered = L->nblocks;  /* to subtract nblocks after gc */
  Hash *freetable;
  TaggedString *freestr;
  TProtoFunc *freefunc;
  Closure *freeclos;
  markall();
  invalidaterefs();
  freestr = luaS_collector();
  freetable = (Hash *)listcollect(&(L->roottable));
  freefunc = (TProtoFunc *)listcollect(&(L->rootproto));
  freeclos = (Closure *)listcollect(&(L->rootcl));
  L->GCthreshold *= 4;  /* to avoid GC during GC */
  luaC_hashcallIM(freetable);  /* GC tag methods for tables */
  luaC_strcallIM(freestr);  /* GC tag methods for userdata */
  luaD_gcIM(&luaO_nilobject);  /* GC tag method for nil (signal end of GC) */
  luaH_free(freetable);
  luaS_free(freestr);
  luaF_freeproto(freefunc);
  luaF_freeclosure(freeclos);
  recovered = recovered-L->nblocks;
  L->GCthreshold = (limit == 0) ? 2*L->nblocks : L->nblocks+limit;
  return recovered;
}


void luaC_checkGC (void)
{
  if (L->nblocks >= L->GCthreshold)
    lua_collectgarbage(0);
}

