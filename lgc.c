/*
** $Id: lgc.c,v 1.63 2000/08/22 17:44:17 roberto Exp roberto $
** Garbage Collector
** See Copyright Notice in lua.h
*/

#include "lua.h"

#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"


typedef struct GCState {
  Hash *tmark;  /* list of marked tables to be visited */
  Closure *cmark;  /* list of marked closures to be visited */
} GCState;



static int markobject (GCState *st, TObject *o);


/* mark a string; marks larger than 1 cannot be changed */
#define strmark(s)    {if ((s)->marked == 0) (s)->marked = 1;}



static void protomark (Proto *f) {
  if (!f->marked) {
    int i;
    f->marked = 1;
    strmark(f->source);
    for (i=0; i<f->nkstr; i++)
      strmark(f->kstr[i]);
    for (i=0; i<f->nkproto; i++)
      protomark(f->kproto[i]);
    for (i=0; i<f->nlocvars; i++)  /* mark local-variable names */
      strmark(f->locvars[i].varname);
  }
}


static void markstack (lua_State *L, GCState *st) {
  StkId o;
  for (o=L->stack; o<L->top; o++)
    markobject(st, o);
}


static void marklock (lua_State *L, GCState *st) {
  int i;
  for (i=0; i<L->refSize; i++) {
    if (L->refArray[i].st == LOCK)
      markobject(st, &L->refArray[i].o);
  }
}


static void marktagmethods (lua_State *L, GCState *st) {
  int e;
  for (e=0; e<IM_N; e++) {
    int t;
    for (t=0; t<=L->last_tag; t++)
      markobject(st, luaT_getim(L, t,e));
  }
}


static int markobject (GCState *st, TObject *o) {
  switch (ttype(o)) {
    case TAG_USERDATA:  case TAG_STRING:
      strmark(tsvalue(o));
      break;
    case TAG_TABLE: {
      if (!ismarked(hvalue(o))) {
        hvalue(o)->mark = st->tmark;  /* chain it in list of marked */
        st->tmark = hvalue(o);
      }
      break;
    }
    case TAG_LMARK: {
      Closure *cl = infovalue(o)->func;
      if (!ismarked(cl)) {
        protomark(cl->f.l);
        cl->mark = st->cmark;  /* chain it for later traversal */
        st->cmark = cl;
      }
      break;
    }
    case TAG_LCLOSURE:
      protomark(clvalue(o)->f.l);
      /* go through */
    case TAG_CCLOSURE:  case TAG_CMARK:
      if (!ismarked(clvalue(o))) {
        clvalue(o)->mark = st->cmark;  /* chain it for later traversal */
        st->cmark = clvalue(o);
      }
      break;
    default: break;  /* numbers, etc */
  }
  return 0;
}


static void markall (lua_State *L) {
  GCState st;
  st.cmark = NULL;
  st.tmark = L->gt;  /* put table of globals in mark list */
  L->gt->mark = NULL;
  marktagmethods(L, &st);  /* mark tag methods */
  markstack(L, &st); /* mark stack objects */
  marklock(L, &st); /* mark locked objects */
  for (;;) {  /* mark tables and closures */
    if (st.cmark) {
      int i;
      Closure *f = st.cmark;  /* get first closure from list */
      st.cmark = f->mark;  /* remove it from list */
      for (i=0; i<f->nupvalues; i++)  /* mark its upvalues */
        markobject(&st, &f->upvalue[i]);
    }
    else if (st.tmark) {
      int i;
      Hash *h = st.tmark;  /* get first table from list */
      st.tmark = h->mark;  /* remove it from list */
      for (i=0; i<h->size; i++) {
        Node *n = node(h, i);
        if (ttype(key(n)) != TAG_NIL) {
          if (ttype(val(n)) == TAG_NIL)
            luaH_remove(h, key(n));  /* dead element; try to remove it */
          markobject(&st, &n->key);
          markobject(&st, &n->val);
        }
      }
    }
    else break;  /* nothing else to mark */
  }
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


/* macro for internal debugging; check if a link of free refs is valid */
#define VALIDLINK(L, st,n)      (NONEXT <= (st) && (st) < (n))

static void invalidaterefs (lua_State *L) {
  int n = L->refSize;
  int i;
  for (i=0; i<n; i++) {
    struct Ref *r = &L->refArray[i];
    if (r->st == HOLD && !hasmark(&r->o))
      r->st = COLLECTED;
    LUA_ASSERT((r->st == LOCK && hasmark(&r->o)) ||
               (r->st == HOLD && hasmark(&r->o)) ||
                r->st == COLLECTED ||
                r->st == NONEXT ||
               (r->st < n && VALIDLINK(L, L->refArray[r->st].st, n)),
               "inconsistent ref table");
  }
  LUA_ASSERT(VALIDLINK(L, L->refFree, n), "inconsistent ref table");
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
    if (ismarked(next)) {
      next->mark = next;  /* unmark */
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
    if (ismarked(next)) {
      next->mark = next;  /* unmark */
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
  invalidaterefs(L);
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

