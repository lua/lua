/*
** $Id: lgc.c,v 1.58 2000/06/26 19:28:31 roberto Exp roberto $
** Garbage Collector
** See Copyright Notice in lua.h
*/

#define LUA_REENTRANT

#include "lua.h"

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




static int markobject (lua_State *L, TObject *o);


/* mark a string; marks larger than 1 cannot be changed */
#define strmark(L, s)    {if ((s)->marked == 0) (s)->marked = 1;}



static void protomark (lua_State *L, Proto *f) {
  if (!f->marked) {
    int i;
    f->marked = 1;
    strmark(L, f->source);
    for (i=0; i<f->nkstr; i++)
      strmark(L, f->kstr[i]);
    for (i=0; i<f->nkproto; i++)
      protomark(L, f->kproto[i]);
    if (f->locvars) {  /* is there debug information? */
      LocVar *lv;
      for (lv=f->locvars; lv->pc != -1; lv++)  /* mark local-variable names */
        if (lv->varname) strmark(L, lv->varname);
    }
  }
}


static void closuremark (lua_State *L, Closure *f) {
  if (!f->marked) {
    int i = f->nupvalues;
    f->marked = 1;
    while (i--)
      markobject(L, &f->upvalue[i]);
  }
}


static void tablemark (lua_State *L, Hash *h) {
  if (!h->marked) {
    int i;
    h->marked = 1;
    for (i=h->size-1; i>=0; i--) {
      Node *n = node(h,i);
      if (ttype(key(n)) != TAG_NIL) {
        if (ttype(val(n)) == TAG_NIL)
          luaH_remove(h, key(n));  /* dead element; try to remove it */
        markobject(L, &n->key);
        markobject(L, &n->val);
      }
    }
  }
}


static void travstack (lua_State *L) {
  StkId o;
  for (o=L->stack; o<L->top; o++)
    markobject(L, o);
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
      tablemark(L, hvalue(o));
      break;
    case TAG_LCLOSURE:
      protomark(L, clvalue(o)->f.l);
      closuremark(L, clvalue(o));
      break;
    case TAG_LMARK: {
      Closure *cl = infovalue(o)->func;
      protomark(L, cl->f.l);
      closuremark(L, cl);
      break;
    }
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


static void checktab (lua_State *L, stringtable *tb) {
  if (tb->nuse < (lint32)(tb->size/4) && tb->size > 10)
    luaS_resize(L, tb, tb->size/2);  /* table is too big */
}


/*
** collect all elements with `marked' <= `limit'.
** with limit=0, that means all unmarked elements;
** with limit=MAX_INT, that means all elements.
*/
static void collectstringtab (lua_State *L, int limit) {
  int i;
  for (i=0; i<L->strt.size; i++) {  /* for each list */
    TString **p = &L->strt.hash[i];
    TString *next;
    while ((next = *p) != NULL) {
      if (next->marked > limit) {  /* preserve? */
        if (next->marked < FIXMARK)  /* does not change FIXMARKs */
          next->marked = 0;
        p = &next->nexthash;
      } 
      else {  /* collect */
        *p = next->nexthash;
        L->strt.nuse--;
        L->nblocks -= gcsizestring(L, next->u.s.len);
        luaM_free(L, next);
      }
    }
  }
  checktab(L, &L->strt);
}


static void collectudatatab (lua_State *L, int all) {
  int i;
  for (i=0; i<L->udt.size; i++) {  /* for each list */
    TString **p = &L->udt.hash[i];
    TString *next;
    while ((next = *p) != NULL) {
      LUA_ASSERT(next->marked <= 1, "udata cannot be fixed");
      if (next->marked > all) {  /* preserve? */
        next->marked = 0;
        p = &next->nexthash;
      } 
      else {  /* collect */
        int tag = next->u.d.tag;
        if (tag > L->last_tag) tag = TAG_USERDATA;
        *p = next->nexthash;
        next->nexthash = L->IMtable[tag].collected;  /* chain udata */
        L->IMtable[tag].collected = next;
        L->nblocks -= gcsizeudata;
        L->udt.nuse--;
      }
    }
  }
  checktab(L, &L->udt);
}


static void callgcTM (lua_State *L, const TObject *o) {
  const TObject *im = luaT_getimbyObj(L, o, IM_GC);
  if (ttype(im) != TAG_NIL) {
    luaD_checkstack(L, 2);
    *(L->top) = *im;
    *(L->top+1) = *o;
    L->top += 2;
    luaD_call(L, L->top-2, 0);
  }
}


static void callgcTMudata (lua_State *L) {
  int tag;
  TObject o;
  ttype(&o) = TAG_USERDATA;
  for (tag=L->last_tag; tag>=0; tag--) {
    TString *udata = L->IMtable[tag].collected;
    L->IMtable[tag].collected = NULL;
    while (udata) {
      TString *next = udata->nexthash;
      tsvalue(&o) = udata;
      callgcTM(L, &o);
      luaM_free(L, udata);
      udata = next;
    }
  }
}


static void markall (lua_State *L) {
  luaT_travtagmethods(L, markobject);  /* mark tag methods */
  travstack(L); /* mark stack objects */
  tablemark(L, L->gt);  /* mark global variables */
  travlock(L); /* mark locked objects */
}


void luaC_collect (lua_State *L, int all) {
  int oldah = L->allowhooks;
  L->allowhooks = 0;  /* stop debug hooks during GC */
  L->GCthreshold *= 4;  /* to avoid GC during GC */
  collectudatatab(L, all);
  callgcTMudata(L);
  collectstringtab(L, all?MAX_INT:0);
  collecttable(L);
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
  callgcTM(L, &luaO_nilobject);
  return recovered;
}


void luaC_checkGC (lua_State *L) {
  if (L->nblocks >= L->GCthreshold)
    lua_collectgarbage(L, 0);
}

