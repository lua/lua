/*
** $Id: lgc.c,v 2.42 2007/10/31 15:41:19 roberto Exp roberto $
** Garbage Collector
** See Copyright Notice in lua.h
*/

#include <string.h>

#define lgc_c
#define LUA_CORE

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


#define GCSTEPSIZE	1024u
#define GCSWEEPMAX	40
#define GCSWEEPCOST	10
#define GCFINALIZECOST	100


#define maskmarks	cast_byte(~(bitmask(BLACKBIT)|WHITEBITS))

#define makewhite(g,x)	\
   ((x)->gch.marked = cast_byte(((x)->gch.marked & maskmarks) | luaC_white(g)))

#define white2gray(x)	reset2bits((x)->gch.marked, WHITE0BIT, WHITE1BIT)
#define black2gray(x)	resetbit((x)->gch.marked, BLACKBIT)

#define stringmark(s)	reset2bits((s)->tsv.marked, WHITE0BIT, WHITE1BIT)


#define isfinalized(u)		testbit((u)->marked, FINALIZEDBIT)
#define markfinalized(u)	l_setbit((u)->marked, FINALIZEDBIT)



#define markvalue(g,o) { checkconsistency(o); \
  if (iscollectable(o) && iswhite(gcvalue(o))) reallymarkobject(g,gcvalue(o)); }

#define markobject(g,t) { if ((t) && iswhite(obj2gco(t))) \
		reallymarkobject(g, obj2gco(t)); }


#define setthreshold(g)  (g->GCthreshold = (g->estimate/100) * g->gcpause)


static void linktable (Table *h, GCObject **p) {
  h->gclist = *p;
  *p = obj2gco(h);
}


static void removeentry (Node *n) {
  lua_assert(ttisnil(gval(n)));
  if (iscollectable(gkey(n)))
    setttype(gkey(n), LUA_TDEADKEY);  /* dead key; remove it */
}


/*
** The next function tells whether a key or value can be cleared from
** a weak table. Non-collectable objects are never removed from weak
** tables. Strings behave as `values', so are never removed too. for
** other objects: if really collected, cannot keep them; for userdata
** being finalized, keep them in keys, but not in values
*/
static int iscleared (const TValue *o, int iskey) {
  if (!iscollectable(o)) return 0;
  if (ttisstring(o)) {
    stringmark(rawtsvalue(o));  /* strings are `values', so are never weak */
    return 0;
  }
  return iswhite(gcvalue(o)) ||
    (ttisuserdata(o) && (!iskey && isfinalized(uvalue(o))));
}


static void reallymarkobject (global_State *g, GCObject *o) {
  lua_assert(iswhite(o) && !isdead(g, o));
  white2gray(o);
  switch (o->gch.tt) {
    case LUA_TSTRING: {
      return;
    }
    case LUA_TUSERDATA: {
      Table *mt = gco2u(o)->metatable;
      gray2black(o);  /* udata are never gray */
      markobject(g, mt);
      markobject(g, gco2u(o)->env);
      return;
    }
    case LUA_TUPVAL: {
      UpVal *uv = gco2uv(o);
      markvalue(g, uv->v);
      if (uv->v == &uv->u.value)  /* closed? */
        gray2black(o);  /* open upvalues are never black */
      return;
    }
    case LUA_TFUNCTION: {
      gco2cl(o)->c.gclist = g->gray;
      g->gray = o;
      break;
    }
    case LUA_TTABLE: {
      linktable(gco2h(o), &g->gray);
      break;
    }
    case LUA_TTHREAD: {
      gco2th(o)->gclist = g->gray;
      g->gray = o;
      break;
    }
    case LUA_TPROTO: {
      gco2p(o)->gclist = g->gray;
      g->gray = o;
      break;
    }
    default: lua_assert(0);
  }
}


static void marktmu (global_State *g) {
  GCObject *u = g->tmudata;
  if (u) {
    do {
      u = u->gch.next;
      makewhite(g, u);  /* may be marked, if left from previous GC */
      reallymarkobject(g, u);
    } while (u != g->tmudata);
  }
}


/* move `dead' udata that need finalization to list `tmudata' */
size_t luaC_separateudata (lua_State *L, int all) {
  global_State *g = G(L);
  size_t deadmem = 0;
  GCObject **p = &g->mainthread->next;
  GCObject *curr;
  while ((curr = *p) != NULL) {
    if (!(iswhite(curr) || all) || isfinalized(gco2u(curr)))
      p = &curr->gch.next;  /* don't bother with them */
    else if (fasttm(L, gco2u(curr)->metatable, TM_GC) == NULL) {
      markfinalized(gco2u(curr));  /* don't need finalization */
      p = &curr->gch.next;
    }
    else {  /* must call its gc method */
      deadmem += sizeudata(gco2u(curr));
      markfinalized(gco2u(curr));
      *p = curr->gch.next;
      /* link `curr' at the end of `tmudata' list */
      if (g->tmudata == NULL)  /* list is empty? */
        g->tmudata = curr->gch.next = curr;  /* creates a circular list */
      else {
        curr->gch.next = g->tmudata->gch.next;
        g->tmudata->gch.next = curr;
        g->tmudata = curr;
      }
    }
  }
  return deadmem;
}


static void traverseweakvalue (global_State *g, Table *h) {
  int i = sizenode(h);
  while (i--) {
    Node *n = gnode(h, i);
    lua_assert(ttype(gkey(n)) != LUA_TDEADKEY || ttisnil(gval(n)));
    if (ttisnil(gval(n)))
      removeentry(n);  /* remove empty entries */
    else {
      lua_assert(!ttisnil(gkey(n)));
      markvalue(g, gkey(n));
    }
  }
  linktable(h, &g->weak);
}


static int traverseephemeron (global_State *g, Table *h) {
  int marked = 0;
  int hasclears = 0;
  int i = h->sizearray;
  while (i--) {  /* mark array part (numeric keys are 'strong') */
    if (iscollectable(&h->array[i]) && iswhite(gcvalue(&h->array[i]))) {
      marked = 1;
      reallymarkobject(g, gcvalue(&h->array[i]));
    }
  }
  i = sizenode(h);
  while (i--) {
    Node *n = gnode(h, i);
    lua_assert(ttype(gkey(n)) != LUA_TDEADKEY || ttisnil(gval(n)));
    if (ttisnil(gval(n)))  /* entry is empty? */
      removeentry(n);  /* remove it */
    else if (iscollectable(gval(n)) && iswhite(gcvalue(gval(n)))) {
      /* value is not marked yet */
      if (iscleared(key2tval(n), 1))  /* key is not marked (yet)? */
        hasclears = 1;  /* may have to propagate mark from key to value */
      else {  /* mark value only if key is marked */
        marked = 1;  /* some mark changed status */
        reallymarkobject(g, gcvalue(gval(n)));
      }
    }
  }
  if (hasclears)
    linktable(h, &g->ephemeron);
  else  /* nothing to propagate */
    linktable(h, &g->weak);  /* avoid convergence phase  */
  return marked;
}


static void traversestrongtable (global_State *g, Table *h) {
  int i;
  i = h->sizearray;
  while (i--)
    markvalue(g, &h->array[i]);
  i = sizenode(h);
  while (i--) {
    Node *n = gnode(h, i);
    lua_assert(ttype(gkey(n)) != LUA_TDEADKEY || ttisnil(gval(n)));
    if (ttisnil(gval(n)))
      removeentry(n);  /* remove empty entries */
    else {
      lua_assert(!ttisnil(gkey(n)));
      markvalue(g, gkey(n));
      markvalue(g, gval(n));
    }
  }
}


static void traversetable (global_State *g, Table *h) {
  const TValue *mode = gfasttm(g, h->metatable, TM_MODE);
  markobject(g, h->metatable);
  if (mode && ttisstring(mode)) {  /* is there a weak mode? */
    int weakkey = (strchr(svalue(mode), 'k') != NULL);
    int weakvalue = (strchr(svalue(mode), 'v') != NULL);
    if (weakkey || weakvalue) {  /* is really weak? */
      black2gray(obj2gco(h));  /* keep table gray */
      if (!weakkey)  /* strong keys? */
        traverseweakvalue(g, h);
      else if (!weakvalue)  /* strong values? */
        traverseephemeron(g, h);
      else {
        lua_assert(weakkey && weakvalue);  /* nothing to traverse now */
        linktable(h, &g->allweak);
      }
      return;
    }  /* else go through */
  }
  traversestrongtable(g, h);
}


/*
** All marks are conditional because a GC may happen while the
** prototype is still being created
*/
static void traverseproto (global_State *g, Proto *f) {
  int i;
  if (f->source) stringmark(f->source);
  for (i=0; i<f->sizek; i++)  /* mark literals */
    markvalue(g, &f->k[i]);
  for (i=0; i<f->sizeupvalues; i++) {  /* mark upvalue names */
    if (f->upvalues[i])
      stringmark(f->upvalues[i]);
  }
  for (i=0; i<f->sizep; i++)  /* mark nested protos */
    markobject(g, f->p[i]);
  for (i=0; i<f->sizelocvars; i++) {  /* mark local-variable names */
    if (f->locvars[i].varname)
      stringmark(f->locvars[i].varname);
  }
}



static void traverseclosure (global_State *g, Closure *cl) {
  markobject(g, cl->c.env);
  if (cl->c.isC) {
    int i;
    for (i=0; i<cl->c.nupvalues; i++)  /* mark its upvalues */
      markvalue(g, &cl->c.upvalue[i]);
  }
  else {
    int i;
    lua_assert(cl->l.nupvalues == cl->l.p->nups);
    markobject(g, cl->l.p);
    for (i=0; i<cl->l.nupvalues; i++)  /* mark its upvalues */
      markobject(g, cl->l.upvals[i]);
  }
}


static void checkstacksizes (lua_State *L, StkId max) {
  /* should not change the stack when  handling overflow or
     during an emergency gc cycle */
  if (L->size_ci > LUAI_MAXCALLS || G(L)->gckind == KGC_EMERGENCY)
    return;  /* do not touch the stacks */
  else {
    int ci_used = cast_int(L->ci - L->base_ci) + 1;  /* number of `ci' in use */
    int s_used = cast_int(max - L->stack) + 1;  /* part of stack in use */
    if (2*ci_used < L->size_ci)
      luaD_reallocCI(L, 2*ci_used);
    if (2*s_used < (L->stacksize - EXTRA_STACK))
      luaD_reallocstack(L, 2*s_used);
  }
}


static void traversestack (global_State *g, lua_State *l) {
  StkId o, lim;
  CallInfo *ci;
  if (l->stack == NULL || l->base_ci == NULL)
    return;  /* stack not completely built yet */
  markvalue(g, gt(l));
  lim = l->top;
  for (ci = l->base_ci; ci <= l->ci; ci++) {
    lua_assert(ci->top <= l->stack_last);
    if (lim < ci->top) lim = ci->top;
  }
  for (o = l->stack; o < l->top; o++)
    markvalue(g, o);
  for (; o <= lim; o++)
    setnilvalue(o);
  checkstacksizes(l, lim);
}


/*
** traverse one gray object, turning it to black.
** Returns `quantity' traversed.
*/
static l_mem propagatemark (global_State *g) {
  GCObject *o = g->gray;
  lua_assert(isgray(o));
  gray2black(o);
  switch (o->gch.tt) {
    case LUA_TTABLE: {
      Table *h = gco2h(o);
      g->gray = h->gclist;
      traversetable(g, h);
      return sizeof(Table) + sizeof(TValue) * h->sizearray +
                             sizeof(Node) * sizenode(h);
    }
    case LUA_TFUNCTION: {
      Closure *cl = gco2cl(o);
      g->gray = cl->c.gclist;
      traverseclosure(g, cl);
      return (cl->c.isC) ? sizeCclosure(cl->c.nupvalues) :
                           sizeLclosure(cl->l.nupvalues);
    }
    case LUA_TTHREAD: {
      lua_State *th = gco2th(o);
      g->gray = th->gclist;
      th->gclist = g->grayagain;
      g->grayagain = o;
      black2gray(o);
      traversestack(g, th);
      return sizeof(lua_State) + sizeof(TValue) * th->stacksize +
                                 sizeof(CallInfo) * th->size_ci;
    }
    case LUA_TPROTO: {
      Proto *p = gco2p(o);
      g->gray = p->gclist;
      traverseproto(g, p);
      return sizeof(Proto) + sizeof(Instruction) * p->sizecode +
                             sizeof(Proto *) * p->sizep +
                             sizeof(TValue) * p->sizek +
                             sizeof(int) * p->sizelineinfo +
                             sizeof(LocVar) * p->sizelocvars +
                             sizeof(TString *) * p->sizeupvalues;
    }
    default: lua_assert(0); return 0;
  }
}


static size_t propagateall (global_State *g) {
  size_t m = 0;
  while (g->gray) m += propagatemark(g);
  return m;
}


static void convergeephemerons (global_State *g) {
  int changed;
  do {
    GCObject *w;
    GCObject *next = g->ephemeron;
    g->ephemeron = NULL;
    changed = 0;
    while ((w = next) != NULL) {
      next = gco2h(w)->gclist;
      if (traverseephemeron(g, gco2h(w))) {
        changed = 1;
        propagateall(g);
      }
    }
  } while (changed);
}



/*
** clear collected entries from weaktables
*/
static void cleartable (GCObject *l) {
  while (l) {
    Table *h = gco2h(l);
    int i = h->sizearray;
    while (i--) {
      TValue *o = &h->array[i];
      if (iscleared(o, 0))  /* value was collected? */
        setnilvalue(o);  /* remove value */
    }
    i = sizenode(h);
    while (i--) {
      Node *n = gnode(h, i);
      if (!ttisnil(gval(n)) &&  /* non-empty entry? */
          (iscleared(key2tval(n), 1) || iscleared(gval(n), 0))) {
        setnilvalue(gval(n));  /* remove value ... */
        removeentry(n);  /* remove entry from table */
      }
    }
    l = h->gclist;
  }
}


static void freeobj (lua_State *L, GCObject *o) {
  switch (o->gch.tt) {
    case LUA_TPROTO: luaF_freeproto(L, gco2p(o)); break;
    case LUA_TFUNCTION: luaF_freeclosure(L, gco2cl(o)); break;
    case LUA_TUPVAL: luaF_freeupval(L, gco2uv(o)); break;
    case LUA_TTABLE: luaH_free(L, gco2h(o)); break;
    case LUA_TTHREAD: {
      lua_assert(gco2th(o) != L && gco2th(o) != G(L)->mainthread);
      luaE_freethread(L, gco2th(o));
      break;
    }
    case LUA_TSTRING: {
      G(L)->strt.nuse--;
      luaM_freemem(L, o, sizestring(gco2ts(o)));
      break;
    }
    case LUA_TUSERDATA: {
      luaM_freemem(L, o, sizeudata(gco2u(o)));
      break;
    }
    default: lua_assert(0);
  }
}



#define sweepwholelist(L,p)	sweeplist(L,p,MAX_LUMEM)


static GCObject **sweeplist (lua_State *L, GCObject **p, lu_mem count) {
  GCObject *curr;
  global_State *g = G(L);
  int deadmask = otherwhite(g);
  while ((curr = *p) != NULL && count-- > 0) {
    if (curr->gch.tt == LUA_TTHREAD)  /* sweep open upvalues of each thread */
      sweepwholelist(L, &gco2th(curr)->openupval);
    if ((curr->gch.marked ^ WHITEBITS) & deadmask) {  /* not dead? */
      lua_assert(!isdead(g, curr) || testbit(curr->gch.marked, FIXEDBIT));
      makewhite(g, curr);  /* make it white (for next cycle) */
      p = &curr->gch.next;
    }
    else {  /* must erase `curr' */
      lua_assert(isdead(g, curr) || deadmask == bitmask(SFIXEDBIT));
      *p = curr->gch.next;
      if (curr == g->rootgc)  /* is the first element of the list? */
        g->rootgc = curr->gch.next;  /* adjust first */
      freeobj(L, curr);
    }
  }
  return p;
}


static void checkSizes (lua_State *L) {
  global_State *g = G(L);
  if (g->strt.nuse < cast(lu_int32, g->strt.size))
    luaS_resize(L, 1 << luaO_ceillog2(g->strt.nuse));
  luaZ_freebuffer(L, &g->buff);
}


static Udata *udata2finalize (global_State *g) {
  GCObject *o = g->tmudata->gch.next;  /* get first element */
  Udata *udata = rawgco2u(o);
  /* remove udata from `tmudata' */
  if (o == g->tmudata)  /* last element? */
    g->tmudata = NULL;
  else
    g->tmudata->gch.next = udata->uv.next;
  udata->uv.next = g->mainthread->next;  /* return it to `root' list */
  g->mainthread->next = o;
  makewhite(g, o);
  return udata;
}


static void GCTM (lua_State *L) {
  global_State *g = G(L);
  Udata *udata = udata2finalize(g);
  const TValue *tm = fasttm(L, udata->uv.metatable, TM_GC);
  if (tm != NULL) {
    lu_byte oldah = L->allowhook;
    lu_mem oldt = g->GCthreshold;
    L->allowhook = 0;  /* stop debug hooks during GC tag method */
    g->GCthreshold = 2*g->totalbytes;  /* avoid GC steps */
    setobj2s(L, L->top, tm);
    setuvalue(L, L->top+1, udata);
    L->top += 2;
    luaD_call(L, L->top - 2, 0);
    L->allowhook = oldah;  /* restore hooks */
    g->GCthreshold = oldt;  /* restore threshold */
  }
}


/*
** Call all GC tag methods
*/
void luaC_callGCTM (lua_State *L) {
  global_State *g = G(L);
  GCObject *last = g->tmudata;
  GCObject *curr;
  if (last == NULL) return;  /* empty list? */
  do {
    curr = g->tmudata->gch.next;  /* element to be collected */
    GCTM(L);
  } while (curr != last);  /* go only until original last */
  /* do not finalize new udata created during previous finalizations  */
  while (g->tmudata)
    udata2finalize(g);  /* simply remove them from list */
}


void luaC_freeall (lua_State *L) {
  global_State *g = G(L);
  int i;
  g->currentwhite = WHITEBITS | bitmask(SFIXEDBIT);  /* mask to collect all elements */
  sweepwholelist(L, &g->rootgc);
  for (i = 0; i < g->strt.size; i++)  /* free all string lists */
    sweepwholelist(L, &g->strt.hash[i]);
}


static void markmt (global_State *g) {
  int i;
  for (i=0; i<NUM_TAGS; i++)
    markobject(g, g->mt[i]);
}


/* mark root set */
static void markroot (lua_State *L) {
  global_State *g = G(L);
  g->gray = NULL;
  g->grayagain = NULL;
  g->weak = g->ephemeron = g->allweak = NULL;
  markobject(g, g->mainthread);
  /* make global table be traversed before main stack */
  markvalue(g, gt(g->mainthread));
  markvalue(g, registry(L));
  markmt(g);
  g->gcstate = GCSpropagate;
}


static void remarkupvals (global_State *g) {
  UpVal *uv;
  for (uv = g->uvhead.u.l.next; uv != &g->uvhead; uv = uv->u.l.next) {
    lua_assert(uv->u.l.next->u.l.prev == uv && uv->u.l.prev->u.l.next == uv);
    if (isgray(obj2gco(uv)))
      markvalue(g, uv->v);
  }
}


static void atomic (lua_State *L) {
  global_State *g = G(L);
  size_t udsize;  /* total size of userdata to be finalized */
  /* remark occasional upvalues of (maybe) dead threads */
  remarkupvals(g);
  /* traverse objects cautch by write barrier and by 'remarkupvals' */
  propagateall(g);
  /* remark weak tables */
  g->gray = g->weak;
  g->weak = NULL;
  lua_assert(!iswhite(obj2gco(g->mainthread)));
  markobject(g, L);  /* mark running thread */
  markmt(g);  /* mark basic metatables (again) */
  propagateall(g);
  /* remark ephemeron tables */
  g->gray = g->ephemeron;
  g->ephemeron = NULL;
  propagateall(g);
  /* remark gray again */
  g->gray = g->grayagain;
  g->grayagain = NULL;
  propagateall(g);
  convergeephemerons(g);
  udsize = luaC_separateudata(L, 0);  /* separate userdata to be finalized */
  marktmu(g);  /* mark `preserved' userdata */
  udsize += propagateall(g);  /* remark, to propagate `preserveness' */
  convergeephemerons(g);
  /* remove collected objects from weak tables */
  cleartable(g->weak);
  cleartable(g->ephemeron);
  cleartable(g->allweak);
  /* flip current white */
  g->currentwhite = cast_byte(otherwhite(g));
  g->sweepstrgc = 0;
  g->sweepgc = &g->rootgc;
  g->gcstate = GCSsweepstring;
  g->estimate = g->totalbytes - udsize;  /* first estimate */
}


#define correctestimate(g,s)  {lu_mem old = g->totalbytes; s; \
          lua_assert(old >= g->totalbytes); g->estimate -= old - g->totalbytes;}


static l_mem singlestep (lua_State *L) {
  global_State *g = G(L);
  /*lua_checkmemory(L);*/
  switch (g->gcstate) {
    case GCSpause: {
      markroot(L);  /* start a new collection */
      return 0;
    }
    case GCSpropagate: {
      if (g->gray)
        return propagatemark(g);
      else {  /* no more `gray' objects */
        atomic(L);  /* finish mark phase */
        return 0;
      }
    }
    case GCSsweepstring: {
      correctestimate(g, sweepwholelist(L, &g->strt.hash[g->sweepstrgc++]));
      if (g->sweepstrgc >= g->strt.size)  /* nothing more to sweep? */
        g->gcstate = GCSsweep;  /* end sweep-string phase */
      return GCSWEEPCOST;
    }
    case GCSsweep: {
      correctestimate(g, g->sweepgc = sweeplist(L, g->sweepgc, GCSWEEPMAX));
      if (*g->sweepgc == NULL)  /* nothing more to sweep? */
        g->gcstate = GCSfinalize;  /* end sweep phase */
      return GCSWEEPMAX*GCSWEEPCOST;
    }
    case GCSfinalize: {
      if (g->tmudata) {
        GCTM(L);
        if (g->estimate > GCFINALIZECOST)
          g->estimate -= GCFINALIZECOST;
        return GCFINALIZECOST;
      }
      else {
        correctestimate(g, checkSizes(L));
        g->gcstate = GCSpause;  /* end collection */
        g->gcdept = 0;
        return 0;
      }
    }
    default: lua_assert(0); return 0;
  }
}


void luaC_step (lua_State *L) {
  global_State *g = G(L);
  l_mem lim = (GCSTEPSIZE/100) * g->gcstepmul;
  lua_assert(g->gckind == KGC_NORMAL);
  if (lim == 0)
    lim = (MAX_LUMEM-1)/2;  /* no limit */
  g->gcdept += g->totalbytes - g->GCthreshold;
  do {
    lim -= singlestep(L);
    if (g->gcstate == GCSpause)
      break;
  } while (lim > 0);
  if (g->gcstate != GCSpause) {
    if (g->gcdept < GCSTEPSIZE)
      g->GCthreshold = g->totalbytes + GCSTEPSIZE;  /* - lim/g->gcstepmul;*/
    else {
      g->gcdept -= GCSTEPSIZE;
      g->GCthreshold = g->totalbytes;
    }
  }
  else {
    lua_assert(g->totalbytes >= g->estimate);
    setthreshold(g);
  }
}


void luaC_fullgc (lua_State *L, int isemergency) {
  global_State *g = G(L);
  lua_assert(g->gckind == KGC_NORMAL);
  g->gckind = isemergency ? KGC_EMERGENCY : KGC_FORCED;
  if (g->gcstate <= GCSpropagate) {
    /* reset sweep marks to sweep all elements (returning them to white) */
    g->sweepstrgc = 0;
    g->sweepgc = &g->rootgc;
    /* reset other collector lists */
    g->gray = NULL;
    g->grayagain = NULL;
    g->weak = g->ephemeron = g->allweak = NULL;
    g->gcstate = GCSsweepstring;
  }
  lua_assert(g->gcstate != GCSpause && g->gcstate != GCSpropagate);
  /* finish any pending sweep phase */
  while (g->gcstate != GCSfinalize) {
    lua_assert(g->gcstate == GCSsweepstring || g->gcstate == GCSsweep);
    singlestep(L);
  }
  markroot(L);
  /* run collector up to finalizers */
  while (g->gcstate != GCSfinalize)
    singlestep(L);
  g->gckind = KGC_NORMAL;
  if (!isemergency) {  /* do not run finalizers during emergency GC */
    while (g->gcstate != GCSpause)
      singlestep(L);
  }
  setthreshold(g);
}


void luaC_barrierf (lua_State *L, GCObject *o, GCObject *v) {
  global_State *g = G(L);
  lua_assert(isblack(o) && iswhite(v) && !isdead(g, v) && !isdead(g, o));
  lua_assert(g->gcstate != GCSfinalize && g->gcstate != GCSpause);
  lua_assert(ttype(&o->gch) != LUA_TTABLE);
  /* must keep invariant? */
  if (g->gcstate == GCSpropagate)
    reallymarkobject(g, v);  /* restore invariant */
  else  /* don't mind */
    makewhite(g, o);  /* mark as white just to avoid other barriers */
}


void luaC_barrierback (lua_State *L, Table *t) {
  global_State *g = G(L);
  GCObject *o = obj2gco(t);
  lua_assert(isblack(o) && !isdead(g, o));
  lua_assert(g->gcstate != GCSfinalize && g->gcstate != GCSpause);
  black2gray(o);  /* make table gray (again) */
  t->gclist = g->grayagain;
  g->grayagain = o;
}


void luaC_link (lua_State *L, GCObject *o, lu_byte tt) {
  global_State *g = G(L);
  o->gch.next = g->rootgc;
  g->rootgc = o;
  o->gch.marked = luaC_white(g);
  o->gch.tt = tt;
}


void luaC_linkupval (lua_State *L, UpVal *uv) {
  global_State *g = G(L);
  GCObject *o = obj2gco(uv);
  o->gch.next = g->rootgc;  /* link upvalue into `rootgc' list */
  g->rootgc = o;
  if (isgray(o)) {
    if (g->gcstate == GCSpropagate) {
      gray2black(o);  /* closed upvalues need barrier */
      luaC_barrier(L, uv, uv->v);
    }
    else {  /* sweep phase: sweep it (turning it into white) */
      makewhite(g, o);
      lua_assert(g->gcstate != GCSfinalize && g->gcstate != GCSpause);
    }
  }
}

