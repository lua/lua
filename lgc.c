/*
** $Id: lgc.c,v 1.136 2002/05/08 17:34:23 roberto Exp roberto $
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


/* mark a string; marks larger than 1 cannot be changed */
#define strmark(s)    {if ((s)->tsv.marked == 0) (s)->tsv.marked = 1;}


/* mark tricks for userdata */
#define isudmarked(u)	(u->uv.len & 1)
#define markud(u)	(u->uv.len |= 1)
#define unmarkud(u)	(u->uv.len &= (~(size_t)1))

#define isfinalized(u)		(u->uv.len & 2)
#define markfinalized(u)	(u->uv.len |= 2)


/* mark tricks for upvalues (assume that open upvalues are always marked) */
#define isupvalmarked(uv)	((uv)->v != &(uv)->value)



#define ismarkable(o)	(!((1 << ttype(o)) & \
	((1 << LUA_TNIL)     | (1 << LUA_TNUMBER) | \
	 (1 << LUA_TBOOLEAN) | (1 << LUA_TUDATAVAL))))

static void reallymarkobject (GCState *st, TObject *o);

#define markobject(st,o)	if (ismarkable(o)) reallymarkobject(st,o)


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


static void marktable (GCState *st, Table *h) {
  if (!ismarked(h)) {
    h->mark = st->tmark;  /* chain it for later traversal */
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
  if (4*used < L->stacksize && 2*BASIC_STACK_SIZE < L->stacksize)
    luaD_reallocstack(L, L->stacksize/2);  /* still big enough... */
}


static void markstacks (GCState *st) {
  lua_State *L1 = st->L;
  do {  /* for each thread */
    StkId o, lim;
    CallInfo *ci;
    if (L1->base_ci == NULL) {  /* incomplete state? */
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
  lua_assert(ttype(val(n)) == LUA_TNIL);
  if (ttype(key(n)) != LUA_TNIL && ttype(key(n)) != LUA_TNUMBER)
    setttype(key(n), LUA_TNONE);  /* dead key; remove it */
}


static void traversetable (GCState *st, Table *h) {
  int i;
  const TObject *mode;
  int weakkey = 0;
  int weakvalue = 0;
  marktable(st, h->metatable);
  lua_assert(h->lsizenode || h->node == G(st->L)->dummynode);
  mode = fasttm(st->L, h->metatable, TM_WEAKMODE);
  if (mode && ttype(mode) == LUA_TSTRING) {  /* weak table? */
    h->mark = st->toclear;  /* must be cleared after GC, ... */
    st->toclear = h;  /* ...put in the appropriate list */
    weakkey = (strchr(svalue(mode), 'k') != NULL);
    weakvalue = (strchr(svalue(mode), 'v') != NULL);
  }
  if (!weakvalue) {
    i = sizearray(h);
    while (i--)
      markobject(st, &h->array[i]);
  }
  i = sizenode(h);
  while (i--) {
    Node *n = node(h, i);
    if (ttype(val(n)) != LUA_TNIL) {
      lua_assert(ttype(key(n)) != LUA_TNIL);
      if (!weakkey) markobject(st, key(n));
      if (!weakvalue) markobject(st, val(n));
    }
  }
}


static void propagatemarks (GCState *st) {
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
  while ((curr = *p) != NULL) {
    if (isudmarked(curr)) {
      unmarkud(curr);
      p = &curr->uv.next;
    }
    else {
      *p = curr->uv.next;
      lua_assert(isfinalized(curr) ||
                 fasttm(L, curr->uv.metatable, TM_GC) == NULL);
      luaM_free(L, curr, sizeudata(curr->uv.len & (~(size_t)3)));
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
  int oldah = L->allowhooks;
  L->allowhooks = 0;  /* stop debug hooks during GC tag methods */
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
  L->allowhooks = oldah;  /* restore hooks */
}


void luaC_callallgcTM (lua_State *L) {
  separateudata(L);
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
  st.L = L;
  st.tmark = NULL;
  st.toclear = NULL;
  markstacks(&st); /* mark all stacks */
  propagatemarks(&st);  /* mark all reachable objects */
  separateudata(L);  /* separate userdata to be preserved */
  marktmu(&st);  /* mark `preserved' userdata */
  propagatemarks(&st);  /* remark */
  cleartables(st.toclear);
  luaC_collect(L, 0);
  checkMbuffer(L);
  G(L)->GCthreshold = 2*G(L)->nblocks;  /* new threshold */
  callGCTM(L);
}

