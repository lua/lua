/*
** $Id: lgc.c,v 1.148 2002/08/30 19:09:21 roberto Exp roberto $
** Garbage Collector
** See Copyright Notice in lua.h
*/

#include <string.h>

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
  lua_State *L;
} GCState;


/*
** some userful bit tricks
*/
#define setbit(x,b)	((x) |= (1<<(b)))
#define resetbit(x,b)	((x) &= cast(lu_byte, ~(1<<(b))))
#define testbit(x,b)	((x) & (1<<(b)))

#define mark(x)		setbit((x)->gch.marked, 0)
#define unmark(x)	resetbit((x)->gch.marked, 0)
#define ismarked(x)	((x)->gch.marked & ((1<<4)|1))

#define strmark(s)	setbit((s)->tsv.marked, 0)
#define strunmark(s)	resetbit((s)->tsv.marked, 0)


#define isfinalized(u)		(!testbit((u)->uv.marked, 1))
#define markfinalized(u)	resetbit((u)->uv.marked, 1)


static void reallymarkobject (GCState *st, GCObject *o);

#define markobject(st,o) { checkconsistency(o); \
  if (iscollectable(o) && !ismarked(gcvalue(o))) reallymarkobject(st,gcvalue(o)); }

#define condmarkobject(st,o,c) { checkconsistency(o); \
  if (iscollectable(o) && !ismarked(gcvalue(o)) && (c)) \
    reallymarkobject(st,gcvalue(o)); }

#define marktable(st,t) { if (!ismarked(cast(GCObject *, t))) \
		reallymarkobject(st, cast(GCObject *, (t))); }


static void markproto (Proto *f) {
  if (!f->marked) {
    int i;
    f->marked = 1;
    strmark(f->source);
    for (i=0; i<f->sizek; i++) {
      if (ttisstring(f->k+i))
        strmark(tsvalue(f->k+i));
    }
    for (i=0; i<f->sizep; i++)
      markproto(f->p[i]);
    for (i=0; i<f->sizelocvars; i++)  /* mark local-variable names */
      strmark(f->locvars[i].varname);
  }
  lua_assert(luaG_checkcode(f));
}



static void markclosure (GCState *st, Closure *cl) {
  if (cl->c.isC) {
    int i;
    for (i=0; i<cl->c.nupvalues; i++)  /* mark its upvalues */
      markobject(st, &cl->c.upvalue[i]);
  }
  else {
    int i;
    lua_assert(cl->l.nupvalues == cl->l.p->nupvalues);
    marktable(st, hvalue(&cl->l.g));
    markproto(cl->l.p);
    for (i=0; i<cl->l.nupvalues; i++) {  /* mark its upvalues */
      UpVal *u = cl->l.upvals[i];
      if (!u->marked) {
        markobject(st, &u->value);
        u->marked = 1;
      }
    }
  }
}


static void reallymarkobject (GCState *st, GCObject *o) {
  mark(o);
  switch (o->gch.tt) {
    case LUA_TFUNCTION: {
      markclosure(st, &o->cl);
      break;
    }
    case LUA_TUSERDATA: {
      marktable(st, (&o->u)->uv.metatable);
      break;
    }
    case LUA_TTABLE: {
      (&o->h)->gclist = st->tmark;  /* chain it for later traversal */
      st->tmark = &o->h;
      break;
    }
  }
}


static void checkstacksizes (lua_State *L, StkId max) {
  int used = L->ci - L->base_ci;  /* number of `ci' in use */
  if (4*used < L->size_ci && 2*BASIC_CI_SIZE < L->size_ci)
    luaD_reallocCI(L, L->size_ci/2);  /* still big enough... */
  used = max - L->stack;  /* part of stack in use */
  if (4*used < L->stacksize && 2*(BASIC_STACK_SIZE+EXTRA_STACK) < L->stacksize)
    luaD_reallocstack(L, L->stacksize/2);  /* still big enough... */
}


static void traversestacks (GCState *st) {
  lua_State *L1 = st->L;
  do {  /* for each thread */
    StkId o, lim;
    CallInfo *ci;
    if (ttisnil(defaultmeta(L1))) {  /* incomplete state? */
      lua_assert(L1 != st->L);
      L1 = L1->next;
      luaE_closethread(st->L, L1->previous);  /* collect it */
      continue;
    }
    markobject(st, defaultmeta(L1));
    markobject(st, gt(L1));
    markobject(st, registry(L1));
    for (o=L1->stack; o<L1->top; o++)
      markobject(st, o);
    lim = o;
    for (ci = L1->base_ci; ci <= L1->ci; ci++) {
      lua_assert(ci->top <= L1->stack_last);
      if (lim < ci->top) lim = ci->top;
    }
    for (; o<=lim; o++) setnilvalue(o);
    checkstacksizes(L1, lim);
    lua_assert(L1->previous->next == L1 && L1->next->previous == L1);
    L1 = L1->next;
  } while (L1 != st->L);
}


static void marktmu (GCState *st) {
  GCObject *u;
  for (u = G(st->L)->tmudata; u; u = u->uv.next)
    reallymarkobject(st, u);
}


/* move `dead' udata that need finalization to list `tmudata' */
static void separateudata (lua_State *L) {
  GCObject **p = &G(L)->rootudata;
  GCObject *curr;
  GCObject *collected = NULL;  /* to collect udata with gc event */
  GCObject **lastcollected = &collected;
  while ((curr = *p) != NULL) {
    lua_assert(curr->gch.tt == LUA_TUSERDATA);
    if (ismarked(curr) || isfinalized(&curr->u))
      p = &curr->uv.next;  /* don't bother with them */

    else if (fasttm(L, (&curr->u)->uv.metatable, TM_GC) == NULL) {
      markfinalized(&curr->u);  /* don't need finalization */
      p = &curr->uv.next;
    }
    else {  /* must call its gc method */
      *p = curr->uv.next;
      curr->uv.next = NULL;  /* link `curr' at the end of `collected' list */
      *lastcollected = curr;
      lastcollected = &curr->uv.next;
    }
  }
  /* insert collected udata with gc event into `tmudata' list */
  *lastcollected = G(L)->tmudata;
  G(L)->tmudata = collected;
}


static void removekey (Node *n) {
  setnilvalue(val(n));  /* remove corresponding value ... */
  if (iscollectable(key(n)))
    setttype(key(n), LUA_TNONE);  /* dead key; remove it */
}


static void traversetable (GCState *st, Table *h) {
  int i;
  int weakkey = 0;
  int weakvalue = 0;
  marktable(st, h->metatable);
  lua_assert(h->lsizenode || h->node == G(st->L)->dummynode);
  if (h->mode & (WEAKKEY | WEAKVALUE)) {  /* weak table? */
    weakkey = h->mode & WEAKKEY;
    weakvalue = h->mode & WEAKVALUE;
    h->gclist = st->toclear;  /* must be cleared after GC, ... */
    st->toclear = h;  /* ... so put in the appropriate list */
  }
  if (!weakvalue) {
    i = sizearray(h);
    while (i--)
      markobject(st, &h->array[i]);
  }
  i = sizenode(h);
  while (i--) {
    Node *n = node(h, i);
    if (!ttisnil(val(n))) {
      lua_assert(!ttisnil(key(n)));
      condmarkobject(st, key(n), !weakkey);
      condmarkobject(st, val(n), !weakvalue);
    }
  }
}


static void propagatemarks (GCState *st) {
  while (st->tmark) {  /* traverse marked tables */
    Table *h = st->tmark;  /* get first table from list */
    st->tmark = h->gclist;  /* remove it from list */
    traversetable(st, h);
  }
}


static int valismarked (const TObject *o) {
  if (ttisstring(o))
    strmark(tsvalue(o));  /* strings are `values', so are never weak */
  return !iscollectable(o) || testbit(o->value.gc->gch.marked, 0);
}


/*
** clear collected keys from weaktables
*/
static void cleartablekeys (GCState *st) {
  Table *h;
  for (h = st->toclear; h; h = h->gclist) {
    lua_assert(h->mode & (WEAKKEY | WEAKVALUE));
    if ((h->mode & WEAKKEY)) {  /* table may have collected keys? */
      int i = sizenode(h);
      while (i--) {
        Node *n = node(h, i);
        if (!valismarked(key(n)))  /* key was collected? */
          removekey(n);  /* remove entry from table */
      }
    }
  }
}


/*
** clear collected values from weaktables
*/
static void cleartablevalues (GCState *st) {
  Table *h;
  for (h = st->toclear; h; h = h->gclist) {
    if ((h->mode & WEAKVALUE)) {  /* table may have collected values? */
      int i = sizearray(h);
      while (i--) {
        TObject *o = &h->array[i];
        if (!valismarked(o))  /* value was collected? */
          setnilvalue(o);  /* remove value */
      }
      i = sizenode(h);
      while (i--) {
        Node *n = node(h, i);
        if (!valismarked(val(n)))  /* value was collected? */
          removekey(n);  /* remove entry from table */
      }
    }
  }
}


static void freeobj (lua_State *L, GCObject *o) {
  switch (o->gch.tt) {
    case LUA_TPROTO: luaF_freeproto(L, &o->p); break;
    case LUA_TFUNCTION: luaF_freeclosure(L, &o->cl); break;
    case LUA_TUPVAL: luaM_freelem(L, &o->uv); break;
    case LUA_TTABLE: luaH_free(L, &o->h); break;
    case LUA_TSTRING: {
      luaM_free(L, o, sizestring((&o->ts)->tsv.len));
      break;
    }
    case LUA_TUSERDATA: {
      luaM_free(L, o, sizeudata((&o->u)->uv.len));
      break;
    }
    default: lua_assert(0);
  }
}


static int sweeplist (lua_State *L, GCObject **p, int limit) {
  GCObject *curr;
  int count = 0;  /* number of collected items */
  while ((curr = *p) != NULL) {
    if (curr->gch.marked > limit) {
      unmark(curr);
      p = &curr->gch.next;
    }
    else {
      count++;
      *p = curr->gch.next;
      freeobj(L, curr);
    }
  }
  return count;
}


static void sweepstrings (lua_State *L, int all) {
  int i;
  for (i=0; i<G(L)->strt.size; i++) {  /* for each list */
    G(L)->strt.nuse -= sweeplist(L, &G(L)->strt.hash[i], all);
  }
  if (G(L)->strt.nuse < cast(ls_nstr, G(L)->strt.size/4) &&
      G(L)->strt.size > MINSTRTABSIZE*2)
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
  const TObject *tm = fasttm(L, udata->uv.metatable, TM_GC);
  if (tm != NULL) {
    setobj(L->top, tm);
    setuvalue(L->top+1, udata);
    L->top += 2;
    luaD_call(L, L->top - 2, 0);
  }
}


static void callGCTM (lua_State *L) {
  int oldah = allowhook(L);
  setallowhook(L, 0);  /* stop debug hooks during GC tag methods */
  L->top++;  /* reserve space to keep udata while runs its gc method */
  while (G(L)->tmudata != NULL) {
    GCObject *udata = G(L)->tmudata;
    G(L)->tmudata = udata->uv.next;  /* remove udata from `tmudata' */
    udata->uv.next = G(L)->rootudata;  /* return it to `root' list */
    G(L)->rootudata = udata;
    setuvalue(L->top - 1, &udata->u);  /* keep a reference to it */
    unmark(udata);
    markfinalized(&udata->u);
    do1gcTM(L, &udata->u);
  }
  L->top--;
  setallowhook(L, oldah);  /* restore hooks */
}


void luaC_callallgcTM (lua_State *L) {
  separateudata(L);
  callGCTM(L);  /* call their GC tag methods */
}


void luaC_sweep (lua_State *L, int all) {
  if (all) all = 256;  /* larger than any mark */
  sweeplist(L, &G(L)->rootudata, all);
  sweepstrings(L, all);
  sweeplist(L, &G(L)->rootgc, all);
}


void luaC_collectgarbage (lua_State *L) {
  GCState st;
  st.L = L;
  st.tmark = NULL;
  st.toclear = NULL;
  traversestacks(&st); /* mark all stacks */
  propagatemarks(&st);  /* mark all reachable objects */
  cleartablevalues(&st);
  separateudata(L);  /* separate userdata to be preserved */
  marktmu(&st);  /* mark `preserved' userdata */
  propagatemarks(&st);  /* remark, to propagate `preserveness' */
  cleartablevalues(&st);  /* again, for eventual weak preserved tables */ 
  cleartablekeys(&st);
  luaC_sweep(L, 0);
  checkMbuffer(L);
  G(L)->GCthreshold = 2*G(L)->nblocks;  /* new threshold */
  callGCTM(L);
}


void luaC_link (lua_State *L, GCObject *o, int tt) {
  o->gch.next = G(L)->rootgc;
  G(L)->rootgc = o;
  o->gch.marked = 0;
  o->gch.tt = tt;
}

