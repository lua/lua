/*
** $Id: lgc.c,v 1.180 2003/11/19 19:41:57 roberto Exp roberto $
** Garbage Collector
** See Copyright Notice in lua.h
*/

#include <string.h>

#define lgc_c

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


#define GCSTEPSIZE	(20*sizeof(TObject))



#define isblack(x)	testbit((x)->gch.marked, BLACKBIT)
#define gray2black(x)	((x)->gch.marked++)
#define white2black(x)	setbit((x)->gch.marked, BLACKBIT)

#define iswhite(x)	(!test2bits((x)->gch.marked, GRAYBIT, BLACKBIT))
#define makewhite(x)	reset2bits((x)->gch.marked, GRAYBIT, BLACKBIT)

#define isgray(x)	testbit((x)->gch.marked, GRAYBIT)
#define white2gray(x)	setbit((x)->gch.marked, GRAYBIT)

#define stringmark(s)	setbit((s)->tsv.marked, BLACKBIT)


#define isfinalized(u)		testbit((u)->uv.marked, FINALIZEDBIT)
#define markfinalized(u)	setbit((u)->uv.marked, FINALIZEDBIT)


#define maskbf	bit2mask(BLACKBIT, FIXEDBIT)


#define KEYWEAK         bitmask(KEYWEAKBIT)
#define VALUEWEAK       bitmask(VALUEWEAKBIT)



#define markvalue(g,o) { checkconsistency(o); \
  if (iscollectable(o) && iswhite(gcvalue(o))) reallymarkobject(g,gcvalue(o)); }

#define condmarkobject(g,o,c) { checkconsistency(o); \
  if (iscollectable(o) && iswhite(gcvalue(o)) && (c)) \
    reallymarkobject(g,gcvalue(o)); }

#define markobject(g,t) { if (iswhite(valtogco(t))) \
		reallymarkobject(g, valtogco(t)); }



/*
** computes the size of a collectible object
*/
static size_t objsize (GCObject *o) {
  switch (o->gch.tt) {
    case LUA_TSTRING: {
      TString *ts = gcotots(o);
      return sizestring(ts->tsv.len);
    }
    case LUA_TUSERDATA: {
      Udata *u = gcotou(o);
      return sizeudata(u->uv.len);
    }
    case LUA_TTABLE: {
      Table *h = gcotoh(o);
      return sizeof(Table) + sizeof(TObject) * h->sizearray +
                             sizeof(Node) * sizenode(h);
    }
    case LUA_TUPVAL:
      return sizeof(UpVal);
    case LUA_TFUNCTION: {
      Closure *cl = gcotocl(o);
      return (cl->c.isC) ? sizeCclosure(cl->c.nupvalues) :
                           sizeLclosure(cl->l.nupvalues);
    }
    case LUA_TTHREAD: {
      lua_State *th = gcototh(o);
      return sizeof(lua_State) + sizeof(TObject) * th->stacksize +
             sizeof(CallInfo) * th->size_ci;
    }
    case LUA_TPROTO: {
      Proto *p = gcotop(o);
      return sizeof(Proto) + sizeof(Instruction) * p->sizecode +
             sizeof(Proto *) * p->sizep + sizeof(TObject) * p->sizek + 
             sizeof(int) * p->sizelineinfo + sizeof(LocVar) * p->sizelocvars +
             sizeof(TString *) * p->sizeupvalues;
    }
  }
  lua_assert(0);
  return 0;  /* to avoid warnings */
}


static void reallymarkobject (global_State *g, GCObject *o) {
  lua_assert(iswhite(o));
  switch (o->gch.tt) {
    case LUA_TSTRING: {
      white2black(o);  /* strings do not go to gray list */
      return;
    }
    case LUA_TUSERDATA: {
      white2black(o);  /* userdata do not go to gray list */
      markobject(g, gcotou(o)->uv.metatable);
      return;
    }
    case LUA_TFUNCTION: {
      gcotocl(o)->c.gclist = g->gray;
      break;
    }
    case LUA_TTABLE: {
      gcotoh(o)->gclist = g->gray;
      break;
    }
    case LUA_TTHREAD: {
      gcototh(o)->gclist = g->gray;
      break;
    }
    case LUA_TPROTO: {
      gcotop(o)->gclist = g->gray;
      break;
    }
    case LUA_TUPVAL: {
      gcotouv(o)->gclist = g->gray;
      break;
    }
    default: lua_assert(0);
  }
  g->gray = o;  /* finish list linking */
  white2gray(o);
}


static void marktmu (global_State *g) {
  GCObject *u;
  for (u = g->tmudata; u; u = u->gch.next) {
    makewhite(u);  /* may be marked, if left from previous GC */
    reallymarkobject(g, u);
  }
}


/* move `dead' udata that need finalization to list `tmudata' */
size_t luaC_separateudata (lua_State *L) {
  size_t deadmem = 0;
  GCObject **p = &G(L)->rootudata;
  GCObject *curr;
  GCObject *collected = NULL;  /* to collect udata with gc event */
  GCObject **lastcollected = &collected;
  while ((curr = *p) != NULL) {
    lua_assert(curr->gch.tt == LUA_TUSERDATA);
    if (isblack(curr) || isfinalized(gcotou(curr)))
      p = &curr->gch.next;  /* don't bother with them */

    else if (fasttm(L, gcotou(curr)->uv.metatable, TM_GC) == NULL) {
      markfinalized(gcotou(curr));  /* don't need finalization */
      p = &curr->gch.next;
    }
    else {  /* must call its gc method */
      deadmem += sizeudata(gcotou(curr)->uv.len);
      markfinalized(gcotou(curr));
      *p = curr->gch.next;
      curr->gch.next = NULL;  /* link `curr' at the end of `collected' list */
      *lastcollected = curr;
      lastcollected = &curr->gch.next;
    }
  }
  /* insert collected udata with gc event into `tmudata' list */
  *lastcollected = G(L)->tmudata;
  G(L)->tmudata = collected;
  return deadmem;
}


static void traversetable (global_State *g, Table *h) {
  int i;
  int weakkey = 0;
  int weakvalue = 0;
  const TObject *mode;
  markobject(g, h->metatable);
  lua_assert(h->lsizenode || h->node == g->dummynode);
  mode = gfasttm(g, h->metatable, TM_MODE);
  if (mode && ttisstring(mode)) {  /* is there a weak mode? */
    weakkey = (strchr(svalue(mode), 'k') != NULL);
    weakvalue = (strchr(svalue(mode), 'v') != NULL);
    if (weakkey || weakvalue) {  /* is really weak? */
      h->marked &= ~(KEYWEAK | VALUEWEAK);  /* clear bits */
      h->marked |= cast(lu_byte, (weakkey << KEYWEAKBIT) |
                                 (weakvalue << VALUEWEAKBIT));
      h->gclist = g->weak;  /* must be cleared after GC, ... */
      g->weak = valtogco(h);  /* ... so put in the appropriate list */
    }
  }
  if (weakkey && weakvalue) return;
  if (!weakvalue) {
    i = h->sizearray;
    while (i--)
      markvalue(g, &h->array[i]);
  }
  i = sizenode(h);
  while (i--) {
    Node *n = gnode(h, i);
    if (!ttisnil(gval(n))) {
      lua_assert(!ttisnil(gkey(n)));
      condmarkobject(g, gkey(n), !weakkey);
      condmarkobject(g, gval(n), !weakvalue);
    }
  }
}


/*
** All marks are conditional because a GC may happen while the
** prototype is still being created
*/
static void traverseproto (global_State *g, Proto *f) {
  int i;
  if (f->source) stringmark(f->source);
  for (i=0; i<f->sizek; i++) {  /* mark literal strings */
    if (ttisstring(f->k+i))
      stringmark(tsvalue(f->k+i));
  }
  for (i=0; i<f->sizeupvalues; i++) {  /* mark upvalue names */
    if (f->upvalues[i])
      stringmark(f->upvalues[i]);
  }
  for (i=0; i<f->sizep; i++) {  /* mark nested protos */
    if (f->p[i])
      markobject(g, f->p[i]);
  }
  for (i=0; i<f->sizelocvars; i++) {  /* mark local-variable names */
    if (f->locvars[i].varname)
      stringmark(f->locvars[i].varname);
  }
}



static void traverseclosure (global_State *g, Closure *cl) {
  if (cl->c.isC) {
    int i;
    for (i=0; i<cl->c.nupvalues; i++)  /* mark its upvalues */
      markvalue(g, &cl->c.upvalue[i]);
  }
  else {
    int i;
    lua_assert(cl->l.nupvalues == cl->l.p->nups);
    markobject(g, hvalue(&cl->l.g));
    markobject(g, cl->l.p);
    for (i=0; i<cl->l.nupvalues; i++) {  /* mark its upvalues */
      markobject(g, cl->l.upvals[i]);
    }
  }
}


static void checkstacksizes (lua_State *L, StkId max) {
  int used = L->ci - L->base_ci;  /* number of `ci' in use */
  if (4*used < L->size_ci && 2*BASIC_CI_SIZE < L->size_ci)
    luaD_reallocCI(L, L->size_ci/2);  /* still big enough... */
  else condhardstacktests(luaD_reallocCI(L, L->size_ci));
  used = max - L->stack;  /* part of stack in use */
  if (4*used < L->stacksize && 2*(BASIC_STACK_SIZE+EXTRA_STACK) < L->stacksize)
    luaD_reallocstack(L, L->stacksize/2);  /* still big enough... */
  else condhardstacktests(luaD_reallocstack(L, L->stacksize));
}


static void traversestack (global_State *g, lua_State *L1) {
  StkId o, lim;
  CallInfo *ci;
  markvalue(g, gt(L1));
  lim = L1->top;
  for (ci = L1->base_ci; ci <= L1->ci; ci++) {
    lua_assert(ci->top <= L1->stack_last);
    if (lim < ci->top) lim = ci->top;
  }
  for (o = L1->stack; o < L1->top; o++)
    markvalue(g, o);
  for (; o <= lim; o++)
    setnilvalue(o);
  checkstacksizes(L1, lim);
}


/*
** traverse a given `quantity' of gray objects,
** turning them to black. Returns extra `quantity' traversed.
*/
static l_mem propagatemarks (global_State *g, l_mem lim) {
  GCObject *o;
  while ((o = g->gray) != NULL) {
    lua_assert(isgray(o));
    gray2black(o);
    switch (o->gch.tt) {
      case LUA_TTABLE: {
        Table *h = gcotoh(o);
        g->gray = h->gclist;
        traversetable(g, h);
        break;
      }
      case LUA_TFUNCTION: {
        Closure *cl = gcotocl(o);
        g->gray = cl->c.gclist;
        traverseclosure(g, cl);
        break;
      }
      case LUA_TTHREAD: {
        lua_State *th = gcototh(o);
        g->gray = th->gclist;
        traversestack(g, th);
        break;
      }
      case LUA_TPROTO: {
        Proto *p = gcotop(o);
        g->gray = p->gclist;
        traverseproto(g, p);
        break;
      }
      case LUA_TUPVAL: {
        UpVal *uv = gcotouv(o);
        g->gray = uv->gclist;
        markvalue(g, &uv->value);
        break;
      }
      default: lua_assert(0);
    }
    lim -= objsize(o);
    if (lim <= 0) return lim;
  }
  g->gcstate = GCSatomic;
  return lim;
}


/*
** The next function tells whether a key or value can be cleared from
** a weak table. Non-collectable objects are never removed from weak
** tables. Strings behave as `values', so are never removed too. for
** other objects: if really collected, cannot keep them; for userdata
** being finalized, keep them in keys, but not in values
*/
static int iscleared (const TObject *o, int iskey) {
  if (!iscollectable(o)) return 0;
  if (ttisstring(o)) {
    stringmark(tsvalue(o));  /* strings are `values', so are never weak */
    return 0;
  }
  return !isblack(gcvalue(o)) ||
    (ttisuserdata(o) && (!iskey && isfinalized(uvalue(o))));
}


static void removekey (Node *n) {
  setnilvalue(gval(n));  /* remove corresponding value ... */
  if (iscollectable(gkey(n)))
    setttype(gkey(n), LUA_TNONE);  /* dead key; remove it */
}


/*
** clear collected entries from weaktables
*/
static void cleartable (GCObject *l) {
  while (l) {
    Table *h = gcotoh(l);
    int i = h->sizearray;
    lua_assert(testbit(h->marked, VALUEWEAKBIT) ||
               testbit(h->marked, KEYWEAKBIT));
    if (testbit(h->marked, VALUEWEAKBIT)) {
      while (i--) {
        TObject *o = &h->array[i];
        if (iscleared(o, 0))  /* value was collected? */
          setnilvalue(o);  /* remove value */
      }
    }
    i = sizenode(h);
    while (i--) {
      Node *n = gnode(h, i);
      if (!ttisnil(gval(n)) &&  /* non-empty entry? */
          (iscleared(gkey(n), 1) || iscleared(gval(n), 0)))
        removekey(n);  /* remove entry from table */
    }
    l = h->gclist;
  }
}


static void freeobj (lua_State *L, GCObject *o) {
  switch (o->gch.tt) {
    case LUA_TPROTO: luaF_freeproto(L, gcotop(o)); break;
    case LUA_TFUNCTION: luaF_freeclosure(L, gcotocl(o)); break;
    case LUA_TUPVAL: luaM_freelem(L, gcotouv(o)); break;
    case LUA_TTABLE: luaH_free(L, gcotoh(o)); break;
    case LUA_TTHREAD: {
      lua_assert(gcototh(o) != L && gcototh(o) != G(L)->mainthread);
      luaE_freethread(L, gcototh(o));
      break;
    }
    case LUA_TSTRING: {
      luaM_free(L, o, sizestring(gcotots(o)->tsv.len));
      break;
    }
    case LUA_TUSERDATA: {
      luaM_free(L, o, sizeudata(gcotou(o)->uv.len));
      break;
    }
    default: lua_assert(0);
  }
}


static GCObject **sweeplist (lua_State *L, GCObject **p, int mask,
                             l_mem *plim) {
  GCObject *curr;
  l_mem lim = *plim;
  while ((curr = *p) != NULL) {
    lua_assert(!isgray(curr));
    if (curr->gch.marked & mask) {
      lim -= objsize(curr);
      makewhite(curr);
      p = &curr->gch.next;
    }
    else {
      *p = curr->gch.next;
      freeobj(L, curr);
    }
    if (lim <= 0) break;
  }
  *plim = lim;
  return p;
}


static void sweepstrings (lua_State *L, int mask) {
  int i;
  global_State *g = G(L);
  for (i = 0; i < g->strt.size; i++) {  /* for each list */
    GCObject *curr;
    GCObject **p = &G(L)->strt.hash[i];
    while ((curr = *p) != NULL) {
      lua_assert(!isgray(curr) && curr->gch.tt == LUA_TSTRING);
      if (curr->gch.marked & mask) {
        makewhite(curr);
        p = &curr->gch.next;
      }
      else {
        g->strt.nuse--;
        *p = curr->gch.next;
        luaM_free(L, curr, sizestring(gcotots(curr)->tsv.len));
      }
    }
  }
}


static void checkSizes (lua_State *L) {
  global_State *g = G(L);
  /* check size of string hash */
  if (g->strt.nuse < cast(lu_int32, G(L)->strt.size/4) &&
      g->strt.size > MINSTRTABSIZE*2)
    luaS_resize(L, g->strt.size/2);  /* table is too big */
  /* check size of buffer */
  if (luaZ_sizebuffer(&g->buff) > LUA_MINBUFFER*2) {  /* buffer too big? */
    size_t newsize = luaZ_sizebuffer(&g->buff) / 2;
    luaZ_resizebuffer(L, &g->buff, newsize);
  }
  lua_assert(g->nblocks > g->GCthreshold);
  g->GCthreshold = 2*G(L)->nblocks - g->GCthreshold;  /* new threshold */
}


static void GCTM (lua_State *L) {
  global_State *g = G(L);
  if (g->tmudata == NULL)
    g->gcstate = GCSroot;  /* will restart GC */
  else {
    GCObject *o = g->tmudata;
    Udata *udata = gcotou(o);
    const TObject *tm;
    g->tmudata = udata->uv.next;  /* remove udata from `tmudata' */
    udata->uv.next = g->rootudata;  /* return it to `root' list */
    g->rootudata = o;
    makewhite(o);
    tm = fasttm(L, udata->uv.metatable, TM_GC);
    if (tm != NULL) {
      lu_byte oldah = L->allowhook;
      L->allowhook = 0;  /* stop debug hooks during GC tag method */
      setobj2s(L->top, tm);
      setuvalue(L->top+1, udata);
      L->top += 2;
      luaD_call(L, L->top - 2, 0);
      L->allowhook = oldah;  /* restore hooks */
    }
  }
}


/*
** Call all GC tag methods
*/
void luaC_callGCTM (lua_State *L) {
  while (G(L)->tmudata)
    GCTM(L);
}


void luaC_sweepall (lua_State *L) {
  l_mem dummy = MAXLMEM;
  sweepstrings(L, 0);
  sweeplist(L, &G(L)->rootudata, 0, &dummy);
  sweeplist(L, &G(L)->rootgc, 0, &dummy);
}


/* mark root set */
static void markroot (lua_State *L) {
  global_State *g = G(L);
  lua_assert(g->gray == NULL);
  g->weak = NULL;
  makewhite(valtogco(g->mainthread));
  markobject(g, g->mainthread);
  markvalue(g, defaultmeta(L));
  markvalue(g, registry(L));
  if (L != g->mainthread)  /* another thread is running? */
    markobject(g, L);  /* cannot collect it */
  g->gcstate = GCSpropagate;
}


static void atomic (lua_State *L) {
  global_State *g = G(L);
  g->GCthreshold = luaC_separateudata(L);  /* separate userdata to be preserved */
  marktmu(g);  /* mark `preserved' userdata */
  propagatemarks(g, MAXLMEM);  /* remark, to propagate `preserveness' */
  cleartable(g->weak);  /* remove collected objects from weak tables */
  g->sweepgc = &g->rootgc;
  g->sweepudata = &g->rootudata;
  sweepstrings(L, maskbf);
  g->gcstate = GCSsweep;
}


static void sweepstep (lua_State *L) {
  global_State *g = G(L);
  l_mem lim = GCSTEPSIZE;
  g->sweepudata = sweeplist(L, g->sweepudata, maskbf, &lim);
  g->sweepgc = sweeplist(L, g->sweepgc, maskbf, &lim);
  if (lim == GCSTEPSIZE) {  /* nothing more to sweep? */
    g->gcstate = GCSfinalize;  /* end sweep phase */
  }
}


void luaC_collectgarbage (lua_State *L) {
  global_State *g = G(L);
  /* GCSroot */
  markroot(L);
  /* GCSpropagate */
  while (g->gcstate == GCSpropagate)
    propagatemarks(g, GCSTEPSIZE);
  /* atomic */
  atomic(L);
  /* GCSsweep */
  while (g->gcstate == GCSsweep)
    sweepstep(L);
  /* GCSfinalize */
  checkSizes(L);
  while (g->gcstate == GCSfinalize)
    GCTM(L);
}


void luaC_link (lua_State *L, GCObject *o, lu_byte tt) {
  o->gch.next = G(L)->rootgc;
  G(L)->rootgc = o;
  o->gch.marked = 0;
  o->gch.tt = tt;
}

