/*
** $Id: lgc.c,v 1.113 2001/10/17 21:12:57 roberto Exp $
** Garbage Collector
** See Copyright Notice in lua.h
*/

#define LUA_PRIVATE
#include "lua.h"

#include "ldebug.h"
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
  Table *tmark;  /* list of marked tables to be visited */
} GCState;



/* mark a string; marks larger than 1 cannot be changed */
#define strmark(s)    {if ((s)->tsv.marked == 0) (s)->tsv.marked = 1;}



static void markobject (GCState *st, TObject *o);


static void protomark (Proto *f) {
  if (!f->marked) {
    int i;
    f->marked = 1;
    strmark(f->source);
    for (i=0; i<f->sizek; i++) {
      if (ttype(f->k+i) == LUA_TSTRING)
        strmark(tsvalue(f->k+i));
    }
    for (i=0; i<f->sizep; i++)
      protomark(f->p[i]);
    for (i=0; i<f->sizelocvars; i++)  /* mark local-variable names */
      strmark(f->locvars[i].varname);
  }
  lua_assert(luaG_checkcode(f));
}


static void markclosure (GCState *st, Closure *cl) {
  if (!cl->c.marked) {
    cl->c.marked = 1;
    if (cl->c.isC) {
      int i;
      for (i=0; i<cl->c.nupvalues; i++)  /* mark its upvalues */
        markobject(st, &cl->c.upvalue[i]);
    }
    else {
      int i;
      lua_assert(cl->l.nupvalues == cl->l.p->nupvalues);
      protomark(cl->l.p);
      for (i=0; i<cl->l.nupvalues; i++) {  /* mark its upvalues */
        UpVal *u = cl->l.upvals[i].heap;
        if (u && !u->marked) {
          u->marked = 1;
          markobject(st, &u->val);
        }
      }
    }
  }
}


static void marktable (GCState *st, Table *h) {
  if (!ismarked(h)) {
    h->mark = st->tmark;  /* chain it for later traversal */
    st->tmark = h;
  }
}


static void markobject (GCState *st, TObject *o) {
  switch (ttype(o)) {
    case LUA_TSTRING:
      strmark(tsvalue(o));
      break;
    case LUA_TUSERDATA:
      if (!ismarkedudata(uvalue(o)))
        switchudatamark(uvalue(o));
      break;
    case LUA_TFUNCTION:
      markclosure(st, clvalue(o));
      break;
    case LUA_TTABLE: {
      marktable(st, hvalue(o));
      break;
    }
    default: {
      lua_assert(0 <= ttype(o) && ttype(o) <= LUA_TUPVAL);
      break;
    }
  }
}


static void markstacks (lua_State *L, GCState *st) {
  lua_State *L1 = L;
  do {  /* for each thread */
    StkId o, lim;
    marktable(st, L1->gt);  /* mark table of globals */
    for (o=L1->stack; o<L1->top; o++)
      markobject(st, o);
    lim = (L1->stack_last - L1->ci->base > MAXSTACK) ? L1->ci->base+MAXSTACK
                                                     : L1->stack_last;
    for (; o<=lim; o++) setnilvalue(o);
    lua_assert(L1->previous->next == L1 && L1->next->previous == L1);
    L1 = L1->next;
  } while (L1 != L);
}


static void marktagmethods (global_State *G, GCState *st) {
  int t;
  for (t=0; t<G->ntag; t++) {
    struct TM *tm = &G->TMtable[t];
    int e;
    if (tm->name) strmark(tm->name);
    for (e=0; e<TM_N; e++) {
      Closure *cl = tm->method[e];
      if (cl) markclosure(st, cl);
    }
  }
}


static void removekey (Node *n) {
  lua_assert(ttype(val(n)) == LUA_TNIL);
  if (ttype(key(n)) != LUA_TNIL && ttype(key(n)) != LUA_TNUMBER)
    setttype(key(n), LUA_TNONE);  /* dead key; remove it */
}


static void traversetable (GCState *st, Table *h) {
  int i;
  int mode = h->weakmode;
  if (!(mode & LUA_WEAK_VALUE)) {
    i = sizearray(h);
    while (i--)
      markobject(st, &h->array[i]);
  }
  i = sizenode(h);
  while (i--) {
    Node *n = node(h, i);
    if (ttype(val(n)) != LUA_TNIL) {
      lua_assert(ttype(key(n)) != LUA_TNIL);
      if (!(mode & LUA_WEAK_KEY))
        markobject(st, key(n));
      if (!(mode & LUA_WEAK_VALUE))
        markobject(st, val(n));
    }
  }
}


static void markall (lua_State *L) {
  GCState st;
  st.tmark = NULL;
  marktagmethods(G(L), &st);  /* mark tag methods */
  markstacks(L, &st); /* mark all stacks */
  marktable(&st, G(L)->type2tag);
  markobject(&st, &G(L)->registry);
  while (st.tmark) {  /* mark tables */
    Table *h = st.tmark;  /* get first table from list */
    st.tmark = h->mark;  /* remove it from list */
    traversetable(&st, h);
  }
}


static int hasmark (const TObject *o) {
  switch (ttype(o)) {
    case LUA_TSTRING:
      return tsvalue(o)->tsv.marked;
    case LUA_TUSERDATA:
      return ismarkedudata(uvalue(o));
    case LUA_TTABLE:
      return ismarked(hvalue(o));
    case LUA_TFUNCTION:
      return clvalue(o)->c.marked;
    default:  /* number, nil */
      return 1;
  }
}


static void cleardeadnodes (Table *h) {
  int i;
  i = sizearray(h);
  while (i--) {
    TObject *o = &h->array[i];
    if (!hasmark(o))
      setnilvalue(o);  /* remove value */
  }
  i = sizenode(h);
  while (i--) {
    Node *n = node(h, i);
    if (!hasmark(val(n)) || !hasmark(key(n))) {
      setnilvalue(val(n));  /* remove value ... */
      removekey(n);  /* ... and key */
    }
  }
}


static void cleartables (global_State *G) {
  Table *h;
  for (h = G->roottable; h; h = h->next) {
    if (h->weakmode && ismarked(h))
      cleardeadnodes(h);
  }
}


static void collectproto (lua_State *L) {
  Proto **p = &G(L)->rootproto;
  Proto *curr;
  while ((curr = *p) != NULL) {
    if (curr->marked) {
      curr->marked = 0;
      p = &curr->next;
    }
    else {
      *p = curr->next;
      luaF_freeproto(L, curr);
    }
  }
}


static void collectclosure (lua_State *L, Closure **p) {
  Closure *curr;
  while ((curr = *p) != NULL) {
    if (curr->c.marked) {
      curr->c.marked = 0;
      p = &curr->c.next;
    }
    else {
      *p = curr->c.next;
      luaF_freeclosure(L, curr);
    }
  }
}


static void collectclosures (lua_State *L) {
  lua_State *L1 = L;
  do {  /* for each thread */
    collectclosure(L1, &L1->opencl);
    L1 = L1->next;
  } while (L1 != L);
  collectclosure(L, &G(L)->rootcl);
}


static void collecttable (lua_State *L) {
  Table **p = &G(L)->roottable;
  Table *curr;
  while ((curr = *p) != NULL) {
    if (ismarked(curr)) {
      curr->mark = curr;  /* unmark */
      p = &curr->next;
    }
    else {
      *p = curr->next;
      luaH_free(L, curr);
    }
  }
}


static void collectupval (lua_State *L) {
  UpVal **v = &G(L)->rootupval;
  UpVal *curr;
  while ((curr = *v) != NULL) {
    if (curr->marked) {
      curr->marked = 0;
      v = &curr->next;
    }
    else {
      *v = curr->next;
      luaM_freelem(L, curr);
    }
  }
}


static void collectudata (lua_State *L, int keep) {
  Udata **p = &G(L)->rootudata;
  Udata *curr;
  while ((curr = *p) != NULL) {
    if (ismarkedudata(curr)) {
      switchudatamark(curr);  /* unmark */
      p = &curr->uv.next;
    }
    else {  /* collect */
      int tag = curr->uv.tag;
      *p = curr->uv.next;
      if (keep ||  /* must keep all of them (to close state)? */
          luaT_gettm(G(L), tag, TM_GC)) {  /* or is there a GC tag method? */
        curr->uv.next = G(L)->TMtable[tag].collected;  /* chain udata ... */
        G(L)->TMtable[tag].collected = curr;  /* ... to call its TM later */
      }
      else  /* no tag method; delete udata */
        luaM_free(L, curr, sizeudata(curr->uv.len));
    }
  }
}


static void collectstrings (lua_State *L, int all) {
  int i;
  for (i=0; i<G(L)->strt.size; i++) {  /* for each list */
    TString **p = &G(L)->strt.hash[i];
    TString *curr;
    while ((curr = *p) != NULL) {
      if (curr->tsv.marked && !all) {  /* preserve? */
        if (curr->tsv.marked < FIXMARK)  /* does not change FIXMARKs */
          curr->tsv.marked = 0;
        p = &curr->tsv.nexthash;
      } 
      else {  /* collect */
        *p = curr->tsv.nexthash;
        G(L)->strt.nuse--;
        luaM_free(L, curr, sizestring(curr->tsv.len));
      }
    }
  }
  if (G(L)->strt.nuse < cast(ls_nstr, G(L)->strt.size/4) &&
      G(L)->strt.size > 4)
    luaS_resize(L, G(L)->strt.size/2);  /* table is too big */
}



#define MINBUFFER	256
static void checkMbuffer (lua_State *L) {
  if (G(L)->Mbuffsize > MINBUFFER*2) {  /* is buffer too big? */
    size_t newsize = G(L)->Mbuffsize/2;  /* still larger than MINBUFFER */
    luaM_reallocvector(L, G(L)->Mbuffer, G(L)->Mbuffsize, newsize, l_char);
    G(L)->Mbuffsize = newsize;
  }
}


static void callgcTM (lua_State *L, const TObject *obj) {
  Closure *tm = luaT_gettmbyObj(G(L), obj, TM_GC);
  if (tm != NULL) {
    int oldah = L->allowhooks;
    StkId top = L->top;
    L->allowhooks = 0;  /* stop debug hooks during GC tag methods */
    setclvalue(top, tm);
    setobj(top+1, obj);
    L->top += 2;
    luaD_call(L, top);
    L->top = top;  /* restore top */
    L->allowhooks = oldah;  /* restore hooks */
  }
}


static void callgcTMudata (lua_State *L) {
  int tag;
  luaD_checkstack(L, 3);
  for (tag=G(L)->ntag-1; tag>=0; tag--) {  /* for each tag (in reverse order) */
    Udata *udata;
    while ((udata = G(L)->TMtable[tag].collected) != NULL) {
      G(L)->TMtable[tag].collected = udata->uv.next;  /* remove it from list */
      udata->uv.next = G(L)->rootudata;  /* resurect it */
      G(L)->rootudata = udata;
      setuvalue(L->top, udata);
      L->top++;  /* keep it in stack to avoid being (recursively) collected */
      callgcTM(L, L->top-1);
      uvalue(L->top-1)->uv.tag = 0;  /* default tag (udata is `finalized') */
      L->top--;
    }
  }
}


void luaC_callallgcTM (lua_State *L) {
  if (G(L)->rootudata) {  /* avoid problems with incomplete states */
    collectudata(L, 1);  /* collect all udata into tag lists */
    callgcTMudata(L);  /* call their GC tag methods */
  }
}


void luaC_collect (lua_State *L, int all) {
  collectudata(L, 0);
  collectstrings(L, all);
  collecttable(L);
  collectproto(L);
  collectupval(L);
  collectclosures(L);
}


void luaC_collectgarbage (lua_State *L) {
  markall(L);
  cleartables(G(L));
  luaC_collect(L, 0);
  checkMbuffer(L);
  G(L)->GCthreshold = 2*G(L)->nblocks;  /* new threshold */
  callgcTMudata(L);
  callgcTM(L, &luaO_nilobject);
}

