/*
** $Id: lgc.c,v 1.1 2001/11/29 22:14:34 rieru Exp rieru $
** Garbage Collector
** See Copyright Notice in lua.h
*/

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
  Table *toclear;  /* list of visited weak tables (to be cleared after GC) */
} GCState;



/* mark a string; marks larger than 1 cannot be changed */
#define strmark(s)    {if ((s)->tsv.marked == 0) (s)->tsv.marked = 1;}


/* mark tricks for userdata */
#define isudmarked(u)	(u->uv.len & 1)
#define markud(u)	(u->uv.len |= 1)
#define unmarkud(u)	(u->uv.len--)


/* mark tricks for upvalues (assume that open upvalues are always marked) */
#define isupvalmarked(uv)	((uv)->v != &(uv)->value)


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
        UpVal *u = cl->l.upvals[i];
        if (!isupvalmarked(u)) {
          markobject(st, &u->value);
          u->v = NULL;  /* mark it! */
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
      if (!isudmarked(uvalue(o)))
        markud(uvalue(o));
      break;
    case LUA_TFUNCTION:
      markclosure(st, clvalue(o));
      break;
    case LUA_TTABLE: {
      marktable(st, hvalue(o));
      break;
    }
    default: {
      lua_assert(ttype(o) == LUA_TNIL ||
                 ttype(o) == LUA_TNUMBER ||
                 ttype(o) == LUA_TBOOLEAN);
      break;
    }
  }
}


static void markstacks (lua_State *L, GCState *st) {
  lua_State *L1 = L;
  do {  /* for each thread */
    StkId o, lim;
    for (o=L1->stack; o<L1->top; o++)
      markobject(st, o);
    lim = (L1->stack_last - L1->ci->base > MAXSTACK) ? L1->ci->base+MAXSTACK
                                                     : L1->stack_last;
    for (; o<=lim; o++) setnilvalue(o);
    lua_assert(L1->previous->next == L1 && L1->next->previous == L1);
    L1 = L1->next;
  } while (L1 != L);
}


static void markudet (lua_State *L, GCState *st) {
  Udata *u;
  for (u = G(L)->rootudata; u; u = u->uv.next)
    marktable(st, u->uv.eventtable);
  for (u = G(L)->tmudata; u; u = u->uv.next)
    marktable(st, u->uv.eventtable);
}


static void removekey (Node *n) {
  lua_assert(ttype(val(n)) == LUA_TNIL);
  if (ttype(key(n)) != LUA_TNIL && ttype(key(n)) != LUA_TNUMBER)
    setttype(key(n), LUA_TNONE);  /* dead key; remove it */
}


static void traversetable (GCState *st, Table *h) {
  int i;
  int mode = h->weakmode;
  if (mode) {  /* weak table? must be cleared after GC... */
    h->mark = st->toclear;  /* put in the appropriate list */
    st->toclear = h;
  }
  marktable(st, h->eventtable);
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


static void markall (lua_State *L, GCState *st) {
  markstacks(L, st); /* mark all stacks */
  markudet(L, st);  /* mark userdata's event tables */
  while (st->tmark) {  /* traverse marked tables */
    Table *h = st->tmark;  /* get first table from list */
    st->tmark = h->mark;  /* remove it from list */
    traversetable(st, h);
  }
}


static int hasmark (const TObject *o) {
  switch (ttype(o)) {
    case LUA_TSTRING:
      return tsvalue(o)->tsv.marked;
    case LUA_TUSERDATA:
      return isudmarked(uvalue(o));
    case LUA_TTABLE:
      return ismarked(hvalue(o));
    case LUA_TFUNCTION:
      return clvalue(o)->c.marked;
    default:  /* number, nil, boolean */
      return 1;
  }
}


/*
** clear (set to nil) keys and values from weaktables that were collected
*/
static void cleartables (Table *h) {
  for (; h; h = h->mark) {
    int i;
    lua_assert(h->weakmode);
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


static void collectclosures (lua_State *L) {
  Closure **p = &G(L)->rootcl;
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


static void collectupval (lua_State *L) {
  UpVal **v = &G(L)->rootupval;
  UpVal *curr;
  while ((curr = *v) != NULL) {
    if (isupvalmarked(curr)) {
      lua_assert(curr->v == NULL);
      curr->v = &curr->value;  /* unmark */
      v = &curr->next;  /* next */
    }
    else {
      *v = curr->next;  /* next */
      luaM_freelem(L, curr);
    }
  }
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


static void collectudata (lua_State *L) {
  Udata **p = &G(L)->rootudata;
  Udata *curr;
  Udata *collected = NULL;  /* to collect udata with gc event */
  Udata **lastcollected = &collected;
  while ((curr = *p) != NULL) {
    if (isudmarked(curr)) {
      unmarkud(curr);
      p = &curr->uv.next;
    }
    else {
      *p = curr->uv.next;
      if (fasttm(L, curr->uv.eventtable, TM_GC) != NULL) {  /* gc event? */
        curr->uv.next = NULL;  /* link `curr' at the end of `collected' list */
        *lastcollected = curr;
        lastcollected = &curr->uv.next;
      }
      else  /* no gc event; delete udata */
        luaM_free(L, curr, sizeudata(curr->uv.len));
    }
  }
  /* insert collected udata with gc event into `tmudata' list */
  *lastcollected = G(L)->tmudata;
  G(L)->tmudata = collected;
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
    luaM_reallocvector(L, G(L)->Mbuffer, G(L)->Mbuffsize, newsize, char);
    G(L)->Mbuffsize = newsize;
  }
}


static void do1gcTM (lua_State *L, Udata *udata) {
  const TObject *tm = fasttm(L, udata->uv.eventtable, TM_GC);
  if (tm != NULL) {
    StkId top = L->top;
    setobj(top, tm);
    setuvalue(top+1, udata);
    L->top += 2;
    luaD_call(L, top);
    L->top = top;  /* restore top */
  }
}


static void unprotectedcallGCTM (lua_State *L, void *pu) {
  luaD_checkstack(L, 3);
  L->top++;  /* reserve space to keep udata while runs its gc method */
  while (G(L)->tmudata != NULL) {
    Udata *udata = G(L)->tmudata;
    G(L)->tmudata = udata->uv.next;  /* remove udata from list */
    *(Udata **)pu = udata;  /* keep a reference to it (in case of errors) */
    setuvalue(L->top - 1, udata);  /* and on stack (in case of recursive GC) */
    udata->uv.next = G(L)->rootudata;  /* resurect it */
    G(L)->rootudata = udata;
    do1gcTM(L, udata);
    /* mark udata as finalized (default event table) */
    uvalue(L->top-1)->uv.eventtable = hvalue(defaultet(L));
  }
  L->top--;
}


static void callGCTM (lua_State *L) {
  int oldah = L->allowhooks;
  L->allowhooks = 0;  /* stop debug hooks during GC tag methods */
  while (G(L)->tmudata != NULL) {
    Udata *udata;
    if (luaD_runprotected(L, unprotectedcallGCTM, &udata) != 0) {
      /* `udata' generated an error during its gc */
      /* mark it as finalized (default event table) */
      udata->uv.eventtable = hvalue(defaultet(L));
    }
  }
  L->allowhooks = oldah;  /* restore hooks */
}


void luaC_callallgcTM (lua_State *L) {
  lua_assert(G(L)->tmudata == NULL);
  G(L)->tmudata = G(L)->rootudata;  /* all udata must be collected */
  G(L)->rootudata = NULL;
  callGCTM(L);  /* call their GC tag methods */
}


void luaC_collect (lua_State *L, int all) {
  collectudata(L);
  collectstrings(L, all);
  collecttable(L);
  collectproto(L);
  collectupval(L);
  collectclosures(L);
}


void luaC_collectgarbage (lua_State *L) {
  GCState st;
  st.tmark = NULL;
  st.toclear = NULL;
  markall(L, &st);
  cleartables(st.toclear);
  luaC_collect(L, 0);
  checkMbuffer(L);
  G(L)->GCthreshold = 2*G(L)->nblocks;  /* new threshold */
  callGCTM(L);
}

