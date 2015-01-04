/*
** $Id: lgc.c,v 1.72+ 2000/10/26 12:47:05 roberto Exp $
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



static void markobject (GCState *st, TObject *o);


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


static void markclosure (GCState *st, Closure *cl) {
  if (!ismarked(cl)) {
    if (!cl->isC)
      protomark(cl->f.l);
    cl->mark = st->cmark;  /* chain it for later traversal */
    st->cmark = cl;
  }
}


static void marktagmethods (lua_State *L, GCState *st) {
  int e;
  for (e=0; e<TM_N; e++) {
    int t;
    for (t=0; t<=L->last_tag; t++) {
      Closure *cl = luaT_gettm(L, t, e);
      if (cl) markclosure(st, cl);
    }
  }
}


static void markobject (GCState *st, TObject *o) {
  switch (ttype(o)) {
    case LUA_TUSERDATA:  case LUA_TSTRING:
      strmark(tsvalue(o));
      break;
    case LUA_TMARK:
      markclosure(st, infovalue(o)->func);
      break;
    case LUA_TFUNCTION:
      markclosure(st, clvalue(o));
      break;
    case LUA_TTABLE: {
      if (!ismarked(hvalue(o))) {
        hvalue(o)->mark = st->tmark;  /* chain it in list of marked */
        st->tmark = hvalue(o);
      }
      break;
    }
    default: break;  /* numbers, etc */
  }
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
        if (ttype(key(n)) != LUA_TNIL) {
          if (ttype(val(n)) == LUA_TNIL)
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
    case LUA_TSTRING: case LUA_TUSERDATA:
      return tsvalue(o)->marked;
    case LUA_TTABLE:
      return ismarked(hvalue(o));
    case LUA_TFUNCTION:
      return ismarked(clvalue(o));
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


static void collectstrings (lua_State *L, int all) {
  int i;
  for (i=0; i<L->strt.size; i++) {  /* for each list */
    TString **p = &L->strt.hash[i];
    TString *next;
    while ((next = *p) != NULL) {
      if (next->marked && !all) {  /* preserve? */
        if (next->marked < FIXMARK)  /* does not change FIXMARKs */
          next->marked = 0;
        p = &next->nexthash;
      } 
      else {  /* collect */
        *p = next->nexthash;
        L->strt.nuse--;
        L->nblocks -= sizestring(next->len);
        luaM_free(L, next);
      }
    }
  }
  checktab(L, &L->strt);
}


static void collectudata (lua_State *L, int all) {
  int i;
  for (i=0; i<L->udt.size; i++) {  /* for each list */
    TString **p = &L->udt.hash[i];
    TString *next;
    while ((next = *p) != NULL) {
      LUA_ASSERT(next->marked <= 1, "udata cannot be fixed");
      if (next->marked && !all) {  /* preserve? */
        next->marked = 0;
        p = &next->nexthash;
      } 
      else {  /* collect */
        int tag = next->u.d.tag;
        *p = next->nexthash;
        next->nexthash = L->TMtable[tag].collected;  /* chain udata */
        L->TMtable[tag].collected = next;
        L->nblocks -= sizestring(next->len);
        L->udt.nuse--;
      }
    }
  }
  checktab(L, &L->udt);
}


#define MINBUFFER	256
static void checkMbuffer (lua_State *L) {
  if (L->Mbuffsize > MINBUFFER*2) {  /* is buffer too big? */
    size_t newsize = L->Mbuffsize/2;  /* still larger than MINBUFFER */
    L->nblocks += (newsize - L->Mbuffsize)*sizeof(char);
    L->Mbuffsize = newsize;
    luaM_reallocvector(L, L->Mbuffer, newsize, char);
  }
}


static void callgcTM (lua_State *L, const TObject *o) {
  Closure *tm = luaT_gettmbyObj(L, o, TM_GC);
  if (tm != NULL) {
    int oldah = L->allowhooks;
    L->allowhooks = 0;  /* stop debug hooks during GC tag methods */
    luaD_checkstack(L, 2);
    clvalue(L->top) = tm;
    ttype(L->top) = LUA_TFUNCTION;
    *(L->top+1) = *o;
    L->top += 2;
    luaD_call(L, L->top-2, 0);
    L->allowhooks = oldah;  /* restore hooks */
  }
}


static void callgcTMudata (lua_State *L) {
  int tag;
  TObject o;
  ttype(&o) = LUA_TUSERDATA;
  L->GCthreshold = 2*L->nblocks;  /* avoid GC during tag methods */
  for (tag=L->last_tag; tag>=0; tag--) {  /* for each tag (in reverse order) */
    TString *udata;
    while ((udata = L->TMtable[tag].collected) != NULL) {
      L->TMtable[tag].collected = udata->nexthash;  /* remove it from list */
      tsvalue(&o) = udata;
      callgcTM(L, &o);
      luaM_free(L, udata);
    }
  }
}


void luaC_collect (lua_State *L, int all) {
  collectudata(L, all);
  callgcTMudata(L);
  collectstrings(L, all);
  collecttable(L);
  collectproto(L);
  collectclosure(L);
}


void luaC_collectgarbage (lua_State *L) {
  markall(L);
  invalidaterefs(L);  /* check unlocked references */
  luaC_collect(L, 0);
  checkMbuffer(L);
  L->GCthreshold = 2*L->nblocks;  /* set new threshold */
  callgcTM(L, &luaO_nilobject);
}


void luaC_checkGC (lua_State *L) {
  if (L->nblocks >= L->GCthreshold)
    luaC_collectgarbage(L);
}

