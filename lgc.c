/*
** $Id: lgc.c,v 1.146 2002/08/16 14:45:55 roberto Exp roberto $
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


#define strmark(s)	setbit((s)->tsv.marked, 0)
#define strunmark(s)	resetbit((s)->tsv.marked, 0)



/* mark tricks for userdata */
#define isudmarked(u)	testbit(u->uv.marked, 0)
#define markud(u)	setbit(u->uv.marked, 0)
#define unmarkud(u)	resetbit(u->uv.marked, 0)

#define isfinalized(u)		testbit(u->uv.marked, 1)
#define markfinalized(u)	setbit(u->uv.marked, 1)


#define ismarkable(o)	(!((1 << ttype(o)) & \
	((1 << LUA_TNIL)     | (1 << LUA_TNUMBER) | \
	 (1 << LUA_TBOOLEAN) | (1 << LUA_TLIGHTUSERDATA))))

static void reallymarkobject (GCState *st, TObject *o);

#define markobject(st,o)	if (ismarkable(o)) reallymarkobject(st,o)


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


static void marktable (GCState *st, Table *h) {
  if (!h->marked) {
    h->marked = 1;
    h->gclist = st->tmark;  /* chain it for later traversal */
    st->tmark = h;
  }
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
}


static void markudata (GCState *st, Udata *u) {
  markud(u);
  marktable(st, u->uv.metatable);
}


static void reallymarkobject (GCState *st, TObject *o) {
  switch (ttype(o)) {
    case LUA_TSTRING:
      strmark(tsvalue(o));
      break;
    case LUA_TUSERDATA:
      if (!isudmarked(uvalue(o)))
        markudata(st, uvalue(o));
      break;
    case LUA_TFUNCTION:
      markclosure(st, clvalue(o));
      break;
    case LUA_TTABLE: {
      marktable(st, hvalue(o));
      break;
    }
    default: lua_assert(0);  /* should not be called with other types */
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
  Udata *u;
  for (u = G(st->L)->tmudata; u; u = u->uv.next)
    markudata(st, u);
}


/* move `dead' udata that need finalization to list `tmudata' */
static void separateudata (lua_State *L) {
  Udata **p = &G(L)->rootudata;
  Udata *curr;
  Udata *collected = NULL;  /* to collect udata with gc event */
  Udata **lastcollected = &collected;
  while ((curr = *p) != NULL) {
    if (isudmarked(curr) || isfinalized(curr) ||
        (fasttm(L, curr->uv.metatable, TM_GC) == NULL))
      p = &curr->uv.next;
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
  if (ismarkable(key(n)))
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
      if (!weakkey) markobject(st, key(n));
      if (!weakvalue) markobject(st, val(n));
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


static int ismarked (const TObject *o) {
  switch (ttype(o)) {
    case LUA_TUSERDATA:
      return isudmarked(uvalue(o));
    case LUA_TTABLE:
      return hvalue(o)->marked;
    case LUA_TFUNCTION:
      return clvalue(o)->c.marked;
    case LUA_TSTRING:
      strmark(tsvalue(o));  /* strings are `values', so are never weak */
      /* go through */
    default:  /* number, nil, boolean, udataval */
      return 1;
  }
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
        if (!ismarked(key(n)))  /* key was collected? */
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
        if (!ismarked(o))  /* value was collected? */
          setnilvalue(o);  /* remove value */
      }
      i = sizenode(h);
      while (i--) {
        Node *n = node(h, i);
        if (!ismarked(val(n)))  /* value was collected? */
          removekey(n);  /* remove entry from table */
      }
    }
  }
}


static void sweepproto (lua_State *L) {
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


static void sweepclosures (lua_State *L) {
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


static void sweepupval (lua_State *L) {
  UpVal **v = &G(L)->rootupval;
  UpVal *curr;
  while ((curr = *v) != NULL) {
    if (curr->marked) {
      curr->marked = 0;  /* unmark */
      v = &curr->next;  /* next */
    }
    else {
      *v = curr->next;  /* next */
      luaM_freelem(L, curr);
    }
  }
}


static void sweeptable (lua_State *L) {
  Table **p = &G(L)->roottable;
  Table *curr;
  while ((curr = *p) != NULL) {
    if (curr->marked) {
      curr->marked = 0;
      p = &curr->next;
    }
    else {
      *p = curr->next;
      luaH_free(L, curr);
    }
  }
}



static void sweepudata (lua_State *L) {
  Udata **p = &G(L)->rootudata;
  Udata *curr;
  while ((curr = *p) != NULL) {
    if (isudmarked(curr)) {
      unmarkud(curr);
      p = &curr->uv.next;
    }
    else {
      *p = curr->uv.next;
      luaM_free(L, curr, sizeudata(curr->uv.len));
    }
  }
}


static void sweepstrings (lua_State *L, int all) {
  int i;
  for (i=0; i<G(L)->strt.size; i++) {  /* for each list */
    TString **p = &G(L)->strt.hash[i];
    TString *curr;
    while ((curr = *p) != NULL) {
      if (curr->tsv.marked && !all) {  /* preserve? */
        strunmark(curr);
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
    Udata *udata = G(L)->tmudata;
    G(L)->tmudata = udata->uv.next;  /* remove udata from `tmudata' */
    udata->uv.next = G(L)->rootudata;  /* return it to `root' list */
    G(L)->rootudata = udata;
    setuvalue(L->top - 1, udata);  /* keep a reference to it */
    unmarkud(udata);
    markfinalized(udata);
    do1gcTM(L, udata);
  }
  L->top--;
  setallowhook(L, oldah);  /* restore hooks */
}


void luaC_callallgcTM (lua_State *L) {
  separateudata(L);
  callGCTM(L);  /* call their GC tag methods */
}


void luaC_sweep (lua_State *L, int all) {
  sweepudata(L);
  sweepstrings(L, all);
  sweeptable(L);
  sweepproto(L);
  sweepupval(L);
  sweepclosures(L);
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
  propagatemarks(&st);  /* remark */
  cleartablekeys(&st);
  luaC_sweep(L, 0);
  checkMbuffer(L);
  G(L)->GCthreshold = 2*G(L)->nblocks;  /* new threshold */
  callGCTM(L);
}

