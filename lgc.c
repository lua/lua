/*
** $Id: lgc.c,v 1.47 2000/04/14 18:12:35 roberto Exp roberto $
** Garbage Collector
** See Copyright Notice in lua.h
*/

#define LUA_REENTRANT

#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lref.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lua.h"



static void luaD_gcTM (lua_State *L, const TObject *o) {
  const TObject *im = luaT_getimbyObj(L, o, IM_GC);
  if (ttype(im) != TAG_NIL) {
    luaD_checkstack(L, 2);
    *(L->top++) = *im;
    *(L->top++) = *o;
    luaD_call(L, L->top-2, 0);
  }
}


static int markobject (lua_State *L, TObject *o);


/* mark a string; marks larger than 1 cannot be changed */
#define strmark(L, s)    {if ((s)->marked == 0) (s)->marked = 1;}



static void protomark (lua_State *L, Proto *f) {
  if (!f->marked) {
    int i;
    f->marked = 1;
    strmark(L, f->source);
    for (i=f->nkstr-1; i>=0; i--)
      strmark(L, f->kstr[i]);
    for (i=f->nkproto-1; i>=0; i--)
      protomark(L, f->kproto[i]);
  }
}


static void closuremark (lua_State *L, Closure *f) {
  if (!f->marked) {
    int i = f->nelems;
    f->marked = 1;
    while (i--)
      markobject(L, &f->consts[i]);
  }
}


static void hashmark (lua_State *L, Hash *h) {
  if (!h->marked) {
    int i;
    h->marked = 1;
    for (i=h->size-1; i>=0; i--) {
      Node *n = node(h,i);
      if (ttype(key(n)) != TAG_NIL) {
        markobject(L, &n->key);
        markobject(L, &n->val);
      }
    }
  }
}


static void travstack (lua_State *L) {
  int i;
  for (i = (L->top-1)-L->stack; i>=0; i--)
    markobject(L, L->stack+i);
}


static void travlock (lua_State *L) {
  int i;
  for (i=0; i<L->refSize; i++) {
    if (L->refArray[i].st == LOCK)
      markobject(L, &L->refArray[i].o);
  }
}


static int markobject (lua_State *L, TObject *o) {
  switch (ttype(o)) {
    case TAG_USERDATA:  case TAG_STRING:
      strmark(L, tsvalue(o));
      break;
    case TAG_TABLE:
      hashmark(L, avalue(o));
      break;
    case TAG_LCLOSURE:  case TAG_LMARK:
      protomark(L, clvalue(o)->f.l);
      /* go trhough */
    case TAG_CCLOSURE:  case TAG_CMARK:
      closuremark(L, clvalue(o));
      break;
    default: break;  /* numbers, etc */
  }
  return 0;
}


static void collectproto (lua_State *L) {
  Proto **p = &L->rootproto;
  Proto *next;
  while ((next = *p) != NULL) {
    if (next->marked) {
      next->marked = 0;
      p = &next->next;
    }
    else {
      *p = next->next;
      luaF_freeproto(L, next);
    }
  }
}


static void collectclosure (lua_State *L) {
  Closure **p = &L->rootcl;
  Closure *next;
  while ((next = *p) != NULL) {
    if (next->marked) {
      next->marked = 0;
      p = &next->next;
    }
    else {
      *p = next->next;
      luaF_freeclosure(L, next);
    }
  }
}


static void collecttable (lua_State *L) {
  Hash **p = &L->roottable;
  Hash *next;
  while ((next = *p) != NULL) {
    if (next->marked) {
      next->marked = 0;
      p = &next->next;
    }
    else {
      *p = next->next;
      luaH_free(L, next);
    }
  }
}


/*
** collect all elements with `marked' < `limit'.
** with limit=1, that means all unmarked elements;
** with limit=MAX_INT, that means all elements.
*/
static void collectstring (lua_State *L, int limit) {
  TObject o;  /* to call userdata `gc' tag method */
  int i;
  ttype(&o) = TAG_USERDATA;
  for (i=0; i<NUM_HASHS; i++) {  /* for each hash table */
    stringtable *tb = &L->string_root[i];
    int j;
    for (j=0; j<tb->size; j++) {  /* for each list */
      TString **p = &tb->hash[j];
      TString *next;
      while ((next = *p) != NULL) {
       if (next->marked >= limit) {
         if (next->marked < FIXMARK)  /* does not change FIXMARKs */
           next->marked = 0;
         p = &next->nexthash;
       } 
       else {  /* collect */
          if (next->constindex == -1) {  /* is userdata? */
            tsvalue(&o) = next;
            luaD_gcTM(L, &o);
          }
          *p = next->nexthash;
          luaS_free(L, next);
          tb->nuse--;
        }
      }
    }
    if ((tb->nuse+1)*6 < tb->size)
      luaS_resize(L, tb, tb->size/2);  /* table is too big */
  }
}


static void markall (lua_State *L) {
  travstack(L); /* mark stack objects */
  hashmark(L, L->gt);  /* mark global variable values and names */
  travlock(L); /* mark locked objects */
  luaT_travtagmethods(L, markobject);  /* mark tag methods */
}


void luaC_collect (lua_State *L, int all) {
  int oldah = L->allowhooks;
  L->allowhooks = 0;  /* stop debug hooks during GC */
  L->GCthreshold *= 4;  /* to avoid GC during GC */
  collecttable(L);
  collectstring(L, all?MAX_INT:1);
  collectproto(L);
  collectclosure(L);
  L->allowhooks = oldah;  /* restore hooks */
}


long lua_collectgarbage (lua_State *L, long limit) {
  unsigned long recovered = L->nblocks;  /* to subtract `nblocks' after gc */
  markall(L);
  luaR_invalidaterefs(L);
  luaC_collect(L, 0);
  recovered = recovered - L->nblocks;
  L->GCthreshold = (limit == 0) ? 2*L->nblocks : L->nblocks+limit;
  if (L->Mbuffsize > L->Mbuffnext*4) {  /* is buffer too big? */
    L->Mbuffsize /= 2;  /* still larger than Mbuffnext*2 */
    luaM_reallocvector(L, L->Mbuffer, L->Mbuffsize, char);
  }
  luaD_gcTM(L, &luaO_nilobject);
  return recovered;
}


void luaC_checkGC (lua_State *L) {
  if (L->nblocks >= L->GCthreshold)
    lua_collectgarbage(L, 0);
}

