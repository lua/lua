/*
** $Id: lgc.c,v 2.243 2017/12/20 14:58:05 roberto Exp roberto $
** Garbage Collector
** See Copyright Notice in lua.h
*/

#define lgc_c
#define LUA_CORE

#include "lprefix.h"

#include <stdio.h>
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


/*
** Maximum number of elements to sweep in each single step.
** (Large enough to dissipate fixed overheads but small enough
** to allow small steps for the collector.)
*/
#define GCSWEEPMAX	100

/*
** Maximum number of finalizers to call in each single step.
*/
#define GCFINMAX	10


/*
** Cost of calling one finalizer.
*/
#define GCFINALIZECOST	50


/*
** The equivalent, in bytes, of one unit of "work" (visiting a slot,
** sweeping an object, etc.)
*/
#define WORK2MEM	sizeof(TValue)


/*
** macro to adjust 'pause': 'pause' is actually used like
** 'pause / PAUSEADJ' (value chosen by tests)
*/
#define PAUSEADJ		100


/* mask to erase all color bits (plus gen. related stuff) */
#define maskcolors	(~(bitmask(BLACKBIT) | WHITEBITS | AGEBITS))


/* macro to erase all color bits then sets only the current white bit */
#define makewhite(g,x)	\
 (x->marked = cast_byte((x->marked & maskcolors) | luaC_white(g)))

#define white2gray(x)	resetbits(x->marked, WHITEBITS)
#define black2gray(x)	resetbit(x->marked, BLACKBIT)


#define valiswhite(x)   (iscollectable(x) && iswhite(gcvalue(x)))

#define keyiswhite(n)   (keyiscollectable(n) && iswhite(gckey(n)))


#define checkconsistency(obj)  \
  lua_longassert(!iscollectable(obj) || righttt(obj))


#define markvalue(g,o) { checkconsistency(o); \
  if (valiswhite(o)) reallymarkobject(g,gcvalue(o)); }

#define markkey(g, n)	{ if keyiswhite(n) reallymarkobject(g,gckey(n)); }

#define markobject(g,t)	{ if (iswhite(t)) reallymarkobject(g, obj2gco(t)); }

/*
** mark an object that can be NULL (either because it is really optional,
** or it was stripped as debug info, or inside an uncompleted structure)
*/
#define markobjectN(g,t)	{ if (t) markobject(g,t); }

static void reallymarkobject (global_State *g, GCObject *o);
static lu_mem atomic (lua_State *L);


/*
** {======================================================
** Generic functions
** =======================================================
*/


/*
** one after last element in a hash array
*/
#define gnodelast(h)	gnode(h, cast(size_t, sizenode(h)))


/*
** link collectable object 'o' into list pointed by 'p'
*/
#define linkgclist(o,p)	((o)->gclist = (p), (p) = obj2gco(o))


/*
** Clear keys for empty entries in tables. If entry is empty
** and its key is not marked, mark its entry as dead. This allows the
** collection of the key, but keeps its entry in the table (its removal
** could break a chain). Other places never manipulate dead keys,
** because its associated nil value is enough to signal that the entry
** is logically empty.
*/
static void removeentry (Node *n) {
  lua_assert(ttisnil(gval(n)));
  if (keyiswhite(n))
    setdeadkey(n);  /* unused and unmarked key; remove it */
}


/*
** tells whether a key or value can be cleared from a weak
** table. Non-collectable objects are never removed from weak
** tables. Strings behave as 'values', so are never removed too. for
** other objects: if really collected, cannot keep them; for objects
** being finalized, keep them in keys, but not in values
*/
static int iscleared (global_State *g, const GCObject *o) {
  if (o == NULL) return 0;  /* non-collectable value */
  else if (novariant(o->tt) == LUA_TSTRING) {
    markobject(g, o);  /* strings are 'values', so are never weak */
    return 0;
  }
  else return iswhite(o);
}


/*
** barrier that moves collector forward, that is, mark the white object
** being pointed by a black object. (If in sweep phase, clear the black
** object to white [sweep it] to avoid other barrier calls for this
** same object.)
*/
void luaC_barrier_ (lua_State *L, GCObject *o, GCObject *v) {
  global_State *g = G(L);
  lua_assert(isblack(o) && iswhite(v) && !isdead(g, v) && !isdead(g, o));
  if (keepinvariant(g)) {  /* must keep invariant? */
    reallymarkobject(g, v);  /* restore invariant */
    if (isold(o)) {
      lua_assert(!isold(v));  /* white object could not be old */
      setage(v, G_OLD0);  /* restore generational invariant */
    }
  }
  else {  /* sweep phase */
    lua_assert(issweepphase(g));
    makewhite(g, o);  /* mark main obj. as white to avoid other barriers */
  }
}


/*
** barrier that moves collector backward, that is, mark the black object
** pointing to a white object as gray again.
*/
void luaC_barrierback_ (lua_State *L, Table *t) {
  global_State *g = G(L);
  lua_assert(isblack(t) && !isdead(g, t));
  lua_assert(g->gckind != KGC_GEN || (isold(t) && getage(t) != G_TOUCHED1));
  if (getage(t) != G_TOUCHED2)  /* not already in gray list? */
    linkgclist(t, g->grayagain);  /* link it in 'grayagain' */
  black2gray(t);  /* make table gray (again) */
  setage(t, G_TOUCHED1);  /* touched in current cycle */
}


/*
** Barrier for prototype's cache of closures.  For an 'old1'
** object, making it gray stops it from being visited by 'markold',
** so it is linked in the 'grayagain' list to ensure it will be
** visited. Otherwise, it goes to 'protogray', as only its 'cache' field
** needs to be revisited.  (A prototype to be in this barrier must be
** already finished, so its other fields cannot change and do not need
** to be revisited.)
*/
LUAI_FUNC void luaC_protobarrier_ (lua_State *L, Proto *p) {
  global_State *g = G(L);
  lua_assert(g->gckind != KGC_GEN || isold(p));
  if (getage(p) == G_OLD1)  /* still need to be visited? */
    linkgclist(p, g->grayagain);  /* link it in 'grayagain' */
  else
    linkgclist(p, g->protogray);  /* link it in 'protogray' */
  black2gray(p);  /* make prototype gray (to avoid other barriers) */
}


void luaC_fix (lua_State *L, GCObject *o) {
  global_State *g = G(L);
  lua_assert(g->allgc == o);  /* object must be 1st in 'allgc' list! */
  white2gray(o);  /* they will be gray forever */
  setage(o, G_OLD);  /* and old forever */
  g->allgc = o->next;  /* remove object from 'allgc' list */
  o->next = g->fixedgc;  /* link it to 'fixedgc' list */
  g->fixedgc = o;
}


/*
** create a new collectable object (with given type and size) and link
** it to 'allgc' list.
*/
GCObject *luaC_newobj (lua_State *L, int tt, size_t sz) {
  global_State *g = G(L);
  GCObject *o = cast(GCObject *, luaM_newobject(L, novariant(tt), sz));
  o->marked = luaC_white(g);
  o->tt = tt;
  o->next = g->allgc;
  g->allgc = o;
  return o;
}

/* }====================================================== */



/*
** {======================================================
** Mark functions
** =======================================================
*/


/*
** Mark an object. Userdata, strings, and closed upvalues are visited
** and turned black here. Other objects are marked gray and added
** to appropriate list to be visited (and turned black) later. (Open
** upvalues are already linked in 'headuv' list. They are kept gray
** to avoid barriers, as their values will be revisited by the thread.)
*/
static void reallymarkobject (global_State *g, GCObject *o) {
 reentry:
  white2gray(o);
  switch (o->tt) {
    case LUA_TSHRSTR:
    case LUA_TLNGSTR: {
      gray2black(o);
      break;
    }
    case LUA_TUSERDATA: {
      TValue uvalue;
      markobjectN(g, gco2u(o)->metatable);  /* mark its metatable */
      gray2black(o);
      getuservalue(g->mainthread, gco2u(o), &uvalue);
      if (valiswhite(&uvalue)) {  /* markvalue(g, &uvalue); */
        o = gcvalue(&uvalue);
        goto reentry;
      }
      break;
    }
    case LUA_TUPVAL: {
      UpVal *uv = gco2upv(o);
      if (!upisopen(uv))  /* open upvalues are kept gray */
        gray2black(o);
      markvalue(g, uv->v);  /* mark its content */
      break;
    }
    case LUA_TLCL: {
      linkgclist(gco2lcl(o), g->gray);
      break;
    }
    case LUA_TCCL: {
      linkgclist(gco2ccl(o), g->gray);
      break;
    }
    case LUA_TTABLE: {
      linkgclist(gco2t(o), g->gray);
      break;
    }
    case LUA_TTHREAD: {
      linkgclist(gco2th(o), g->gray);
      break;
    }
    case LUA_TPROTO: {
      linkgclist(gco2p(o), g->gray);
      break;
    }
    default: lua_assert(0); break;
  }
}


/*
** mark metamethods for basic types
*/
static void markmt (global_State *g) {
  int i;
  for (i=0; i < LUA_NUMTAGS; i++)
    markobjectN(g, g->mt[i]);
}


/*
** mark all objects in list of being-finalized
*/
static lu_mem markbeingfnz (global_State *g) {
  GCObject *o;
  lu_mem count = 0;
  for (o = g->tobefnz; o != NULL; o = o->next) {
    count++;
    markobject(g, o);
  }
  return count;
}


/*
** Mark all values stored in marked open upvalues from non-marked threads.
** (Values from marked threads were already marked when traversing the
** thread.) Remove from the list threads that no longer have upvalues and
** not-marked threads.
*/
static int remarkupvals (global_State *g) {
  lua_State *thread;
  lua_State **p = &g->twups;
  int work = 0;
  while ((thread = *p) != NULL) {
    work++;
    lua_assert(!isblack(thread));  /* threads are never black */
    if (isgray(thread) && thread->openupval != NULL)
      p = &thread->twups;  /* keep marked thread with upvalues in the list */
    else {  /* thread is not marked or without upvalues */
      UpVal *uv;
      *p = thread->twups;  /* remove thread from the list */
      thread->twups = thread;  /* mark that it is out of list */
      for (uv = thread->openupval; uv != NULL; uv = uv->u.open.next) {
        work++;
        if (!iswhite(uv))  /* upvalue already visited? */
          markvalue(g, uv->v);  /* mark its value */
      }
    }
  }
  return work;
}


/*
** mark root set and reset all gray lists, to start a new collection
*/
static void restartcollection (global_State *g) {
  g->gray = g->grayagain = NULL;
  g->weak = g->allweak = g->ephemeron = g->protogray = NULL;
  markobject(g, g->mainthread);
  markvalue(g, &g->l_registry);
  markmt(g);
  markbeingfnz(g);  /* mark any finalizing object left from previous cycle */
}

/* }====================================================== */


/*
** {======================================================
** Traverse functions
** =======================================================
*/

/*
** Traverse a table with weak values and link it to proper list. During
** propagate phase, keep it in 'grayagain' list, to be revisited in the
** atomic phase. In the atomic phase, if table has any white value,
** put it in 'weak' list, to be cleared.
*/
static void traverseweakvalue (global_State *g, Table *h) {
  Node *n, *limit = gnodelast(h);
  /* if there is array part, assume it may have white values (it is not
     worth traversing it now just to check) */
  int hasclears = (h->sizearray > 0);
  for (n = gnode(h, 0); n < limit; n++) {  /* traverse hash part */
    if (ttisnil(gval(n)))  /* entry is empty? */
      removeentry(n);  /* remove it */
    else {
      lua_assert(!keyisnil(n));
      markkey(g, n);
      if (!hasclears && iscleared(g, gcvalueN(gval(n))))  /* a white value? */
        hasclears = 1;  /* table will have to be cleared */
    }
  }
  if (g->gcstate == GCSatomic && hasclears)
    linkgclist(h, g->weak);  /* has to be cleared later */
  else
    linkgclist(h, g->grayagain);  /* must retraverse it in atomic phase */
}


/*
** Traverse an ephemeron table and link it to proper list. Returns true
** iff any object was marked during this traversal (which implies that
** convergence has to continue). During propagation phase, keep table
** in 'grayagain' list, to be visited again in the atomic phase. In
** the atomic phase, if table has any white->white entry, it has to
** be revisited during ephemeron convergence (as that key may turn
** black). Otherwise, if it has any white key, table has to be cleared
** (in the atomic phase). In generational mode, it (like all visited
** tables) must be kept in some gray list for post-processing.
*/
static int traverseephemeron (global_State *g, Table *h) {
  int marked = 0;  /* true if an object is marked in this traversal */
  int hasclears = 0;  /* true if table has white keys */
  int hasww = 0;  /* true if table has entry "white-key -> white-value" */
  Node *n, *limit = gnodelast(h);
  unsigned int i;
  /* traverse array part */
  for (i = 0; i < h->sizearray; i++) {
    if (valiswhite(&h->array[i])) {
      marked = 1;
      reallymarkobject(g, gcvalue(&h->array[i]));
    }
  }
  /* traverse hash part */
  for (n = gnode(h, 0); n < limit; n++) {
    if (ttisnil(gval(n)))  /* entry is empty? */
      removeentry(n);  /* remove it */
    else if (iscleared(g, gckeyN(n))) {  /* key is not marked (yet)? */
      hasclears = 1;  /* table must be cleared */
      if (valiswhite(gval(n)))  /* value not marked yet? */
        hasww = 1;  /* white-white entry */
    }
    else if (valiswhite(gval(n))) {  /* value not marked yet? */
      marked = 1;
      reallymarkobject(g, gcvalue(gval(n)));  /* mark it now */
    }
  }
  /* link table into proper list */
  if (g->gcstate == GCSpropagate)
    linkgclist(h, g->grayagain);  /* must retraverse it in atomic phase */
  else if (hasww)  /* table has white->white entries? */
    linkgclist(h, g->ephemeron);  /* have to propagate again */
  else if (hasclears)  /* table has white keys? */
    linkgclist(h, g->allweak);  /* may have to clean white keys */
  else if (g->gckind == KGC_GEN)
    linkgclist(h, g->grayagain);  /* keep it in some list */
  else
    gray2black(h);
  return marked;
}


static void traversestrongtable (global_State *g, Table *h) {
  Node *n, *limit = gnodelast(h);
  unsigned int i;
  for (i = 0; i < h->sizearray; i++)  /* traverse array part */
    markvalue(g, &h->array[i]);
  for (n = gnode(h, 0); n < limit; n++) {  /* traverse hash part */
    if (ttisnil(gval(n)))  /* entry is empty? */
      removeentry(n);  /* remove it */
    else {
      lua_assert(!keyisnil(n));
      markkey(g, n);
      markvalue(g, gval(n));
    }
  }
  if (g->gckind == KGC_GEN) {
    linkgclist(h, g->grayagain);  /* keep it in some gray list */
    black2gray(h);
  }
}


static lu_mem traversetable (global_State *g, Table *h) {
  const char *weakkey, *weakvalue;
  const TValue *mode = gfasttm(g, h->metatable, TM_MODE);
  markobjectN(g, h->metatable);
  if (mode && ttisstring(mode) &&  /* is there a weak mode? */
      ((weakkey = strchr(svalue(mode), 'k')),
       (weakvalue = strchr(svalue(mode), 'v')),
       (weakkey || weakvalue))) {  /* is really weak? */
    black2gray(h);  /* keep table gray */
    if (!weakkey)  /* strong keys? */
      traverseweakvalue(g, h);
    else if (!weakvalue)  /* strong values? */
      traverseephemeron(g, h);
    else  /* all weak */
      linkgclist(h, g->allweak);  /* nothing to traverse now */
  }
  else  /* not weak */
    traversestrongtable(g, h);
  return 1 + h->sizearray + 2 * allocsizenode(h);
}


/*
** Check the cache of a prototype, to keep invariants. If the
** cache is white, clear it. (A cache should not prevent the
** collection of its reference.) Otherwise, if in generational
** mode, check the generational invariant. If the cache is old,
** everything is ok. If the prototype is 'old0', everything
** is ok too. (It will naturally be visited again.) If the
** prototype is older than 'old0', then its cache (which is new)
** must be visited again in the next collection, so the prototype
** goes to the 'protogray' list. (If the prototype has a cache,
** it is already immutable and does not need other barriers;
** then, it can become gray without problems for its other fields.)
*/
static void checkprotocache (global_State *g, Proto *p) {
  if (p->cache) {
    if (iswhite(p->cache))
      p->cache = NULL;  /* allow cache to be collected */
    else if (g->gckind == KGC_GEN && !isold(p->cache) && getage(p) >= G_OLD1) {
      linkgclist(p, g->protogray);  /* link it in 'protogray' */
      black2gray(p);  /* make prototype gray */
    }
  }
  p->cachemiss = 0;  /* restart counting */
}


/*
** Traverse a prototype. (While a prototype is being build, its
** arrays can be larger than needed; the extra slots are filled with
** NULL, so the use of 'markobjectN')
*/
static int traverseproto (global_State *g, Proto *f) {
  int i;
  checkprotocache(g, f);
  markobjectN(g, f->source);
  for (i = 0; i < f->sizek; i++)  /* mark literals */
    markvalue(g, &f->k[i]);
  for (i = 0; i < f->sizeupvalues; i++)  /* mark upvalue names */
    markobjectN(g, f->upvalues[i].name);
  for (i = 0; i < f->sizep; i++)  /* mark nested protos */
    markobjectN(g, f->p[i]);
  for (i = 0; i < f->sizelocvars; i++)  /* mark local-variable names */
    markobjectN(g, f->locvars[i].varname);
  return 1 + f->sizek + f->sizeupvalues + f->sizep + f->sizelocvars;
}


static int traverseCclosure (global_State *g, CClosure *cl) {
  int i;
  for (i = 0; i < cl->nupvalues; i++)  /* mark its upvalues */
    markvalue(g, &cl->upvalue[i]);
  return 1 + cl->nupvalues;
}

/*
** Traverse a Lua closure, marking its prototype and its upvalues.
** (Both can be NULL while closure is being created.)
*/
static int traverseLclosure (global_State *g, LClosure *cl) {
  int i;
  markobjectN(g, cl->p);  /* mark its prototype */
  for (i = 0; i < cl->nupvalues; i++) {  /* visit its upvalues */
    UpVal *uv = cl->upvals[i];
    markobjectN(g, uv);  /* mark upvalue */
  }
  return 1 + cl->nupvalues;
}


/*
** Traverse a thread, marking the elements in the stack up to its top
** and cleaning the rest of the stack in the final traversal.
** That ensures that the entire stack have valid (non-dead) objects.
*/
static int traversethread (global_State *g, lua_State *th) {
  StkId o = th->stack;
  if (o == NULL)
    return 1;  /* stack not completely built yet */
  lua_assert(g->gcstate == GCSatomic ||
             th->openupval == NULL || isintwups(th));
  for (; o < th->top; o++)  /* mark live elements in the stack */
    markvalue(g, s2v(o));
  if (g->gcstate == GCSatomic) {  /* final traversal? */
    StkId lim = th->stack + th->stacksize;  /* real end of stack */
    for (; o < lim; o++)  /* clear not-marked stack slice */
      setnilvalue(s2v(o));
    /* 'remarkupvals' may have removed thread from 'twups' list */
    if (!isintwups(th) && th->openupval != NULL) {
      th->twups = g->twups;  /* link it back to the list */
      g->twups = th;
    }
  }
  else if (!g->gcemergency)
    luaD_shrinkstack(th); /* do not change stack in emergency cycle */
  return 1 + th->stacksize;
}


/*
** traverse one gray object, turning it to black (except for threads,
** which are always gray).
*/
static lu_mem propagatemark (global_State *g) {
  GCObject *o = g->gray;
  gray2black(o);
  switch (o->tt) {
    case LUA_TTABLE: {
      Table *h = gco2t(o);
      g->gray = h->gclist;  /* remove from 'gray' list */
      return traversetable(g, h);
    }
    case LUA_TLCL: {
      LClosure *cl = gco2lcl(o);
      g->gray = cl->gclist;  /* remove from 'gray' list */
      return traverseLclosure(g, cl);
    }
    case LUA_TCCL: {
      CClosure *cl = gco2ccl(o);
      g->gray = cl->gclist;  /* remove from 'gray' list */
      return traverseCclosure(g, cl);
    }
    case LUA_TTHREAD: {
      lua_State *th = gco2th(o);
      g->gray = th->gclist;  /* remove from 'gray' list */
      linkgclist(th, g->grayagain);  /* insert into 'grayagain' list */
      black2gray(o);
      return traversethread(g, th);
    }
    case LUA_TPROTO: {
      Proto *p = gco2p(o);
      g->gray = p->gclist;  /* remove from 'gray' list */
      return traverseproto(g, p);
    }
    default: lua_assert(0); return 0;
  }
}


static lu_mem propagateall (global_State *g) {
  lu_mem tot = 0;
  while (g->gray)
    tot += propagatemark(g);
  return tot;
}


static void convergeephemerons (global_State *g) {
  int changed;
  do {
    GCObject *w;
    GCObject *next = g->ephemeron;  /* get ephemeron list */
    g->ephemeron = NULL;  /* tables may return to this list when traversed */
    changed = 0;
    while ((w = next) != NULL) {
      next = gco2t(w)->gclist;
      if (traverseephemeron(g, gco2t(w))) {  /* traverse marked some value? */
        propagateall(g);  /* propagate changes */
        changed = 1;  /* will have to revisit all ephemeron tables */
      }
    }
  } while (changed);
}

/* }====================================================== */


/*
** {======================================================
** Sweep Functions
** =======================================================
*/

static void clearprotolist (global_State *g) {
  GCObject *p = g->protogray;
  g->protogray = NULL;
  while (p != NULL) {
    Proto *pp = gco2p(p);
    GCObject *next = pp->gclist;
    lua_assert(isgray(pp) && (pp->cache != NULL || pp->cachemiss >= MAXMISS));
    gray2black(pp);
    checkprotocache(g, pp);
    p = next;
  }
}


/*
** clear entries with unmarked keys from all weaktables in list 'l'
*/
static void clearkeys (global_State *g, GCObject *l) {
  for (; l; l = gco2t(l)->gclist) {
    Table *h = gco2t(l);
    Node *n, *limit = gnodelast(h);
    for (n = gnode(h, 0); n < limit; n++) {
      if (!ttisnil(gval(n)) && (iscleared(g, gckeyN(n))))  /* unmarked key? */
        setnilvalue(gval(n));  /* clear value */
      if (ttisnil(gval(n)))  /* is entry empty? */
        removeentry(n);  /* remove it from table */
    }
  }
}


/*
** clear entries with unmarked values from all weaktables in list 'l' up
** to element 'f'
*/
static void clearvalues (global_State *g, GCObject *l, GCObject *f) {
  for (; l != f; l = gco2t(l)->gclist) {
    Table *h = gco2t(l);
    Node *n, *limit = gnodelast(h);
    unsigned int i;
    for (i = 0; i < h->sizearray; i++) {
      TValue *o = &h->array[i];
      if (iscleared(g, gcvalueN(o)))  /* value was collected? */
        setnilvalue(o);  /* remove value */
    }
    for (n = gnode(h, 0); n < limit; n++) {
      if (iscleared(g, gcvalueN(gval(n))))  /* unmarked value? */
        setnilvalue(gval(n));  /* clear value */
      if (ttisnil(gval(n)))  /* is entry empty? */
        removeentry(n);  /* remove it from table */
    }
  }
}


static void freeupval (lua_State *L, UpVal *uv) {
  if (upisopen(uv))
    luaF_unlinkupval(uv);
  luaM_free(L, uv);
}


static void freeobj (lua_State *L, GCObject *o) {
  switch (o->tt) {
    case LUA_TPROTO:
      luaF_freeproto(L, gco2p(o));
      break;
    case LUA_TUPVAL:
      freeupval(L, gco2upv(o));
      break;
    case LUA_TLCL:
      luaM_freemem(L, o, sizeLclosure(gco2lcl(o)->nupvalues));
      break;
    case LUA_TCCL:
      luaM_freemem(L, o, sizeCclosure(gco2ccl(o)->nupvalues));
      break;
    case LUA_TTABLE:
      luaH_free(L, gco2t(o));
      break;
    case LUA_TTHREAD:
      luaE_freethread(L, gco2th(o));
      break;
    case LUA_TUSERDATA:
      luaM_freemem(L, o, sizeudata(gco2u(o)));
      break;
    case LUA_TSHRSTR:
      luaS_remove(L, gco2ts(o));  /* remove it from hash table */
      luaM_freemem(L, o, sizelstring(gco2ts(o)->shrlen));
      break;
    case LUA_TLNGSTR:
      luaM_freemem(L, o, sizelstring(gco2ts(o)->u.lnglen));
      break;
    default: lua_assert(0);
  }
}


/*
** sweep at most 'countin' elements from a list of GCObjects erasing dead
** objects, where a dead object is one marked with the old (non current)
** white; change all non-dead objects back to white, preparing for next
** collection cycle. Return where to continue the traversal or NULL if
** list is finished. ('*countout' gets the number of elements traversed.)
*/
static GCObject **sweeplist (lua_State *L, GCObject **p, int countin,
                             int *countout) {
  global_State *g = G(L);
  int ow = otherwhite(g);
  int i;
  int white = luaC_white(g);  /* current white */
  for (i = 0; *p != NULL && i < countin; i++) {
    GCObject *curr = *p;
    int marked = curr->marked;
    if (isdeadm(ow, marked)) {  /* is 'curr' dead? */
      *p = curr->next;  /* remove 'curr' from list */
      freeobj(L, curr);  /* erase 'curr' */
    }
    else {  /* change mark to 'white' */
      curr->marked = cast_byte((marked & maskcolors) | white);
      p = &curr->next;  /* go to next element */
    }
  }
  if (countout)
    *countout = i;  /* number of elements traversed */
  return (*p == NULL) ? NULL : p;
}


/*
** sweep a list until a live object (or end of list)
*/
static GCObject **sweeptolive (lua_State *L, GCObject **p) {
  GCObject **old = p;
  do {
    p = sweeplist(L, p, 1, NULL);
  } while (p == old);
  return p;
}

/* }====================================================== */


/*
** {======================================================
** Finalization
** =======================================================
*/

/*
** If possible, shrink string table.
*/
static void checkSizes (lua_State *L, global_State *g) {
  if (!g->gcemergency) {
    l_mem olddebt = g->GCdebt;
    if (g->strt.nuse < g->strt.size / 4)  /* string table too big? */
      luaS_resize(L, g->strt.size / 2);
    g->GCestimate += g->GCdebt - olddebt;  /* correct estimate */
  }
}


/*
** Get the next udata to be finalized from the 'tobefnz' list, and
** link it back into the 'allgc' list.
*/
static GCObject *udata2finalize (global_State *g) {
  GCObject *o = g->tobefnz;  /* get first element */
  lua_assert(tofinalize(o));
  g->tobefnz = o->next;  /* remove it from 'tobefnz' list */
  o->next = g->allgc;  /* return it to 'allgc' list */
  g->allgc = o;
  resetbit(o->marked, FINALIZEDBIT);  /* object is "normal" again */
  if (issweepphase(g))
    makewhite(g, o);  /* "sweep" object */
  return o;
}


static void dothecall (lua_State *L, void *ud) {
  UNUSED(ud);
  luaD_callnoyield(L, L->top - 2, 0);
}


static void GCTM (lua_State *L, int propagateerrors) {
  global_State *g = G(L);
  const TValue *tm;
  TValue v;
  setgcovalue(L, &v, udata2finalize(g));
  tm = luaT_gettmbyobj(L, &v, TM_GC);
  if (tm != NULL && ttisfunction(tm)) {  /* is there a finalizer? */
    int status;
    lu_byte oldah = L->allowhook;
    int running  = g->gcrunning;
    L->allowhook = 0;  /* stop debug hooks during GC metamethod */
    g->gcrunning = 0;  /* avoid GC steps */
    setobj2s(L, L->top, tm);  /* push finalizer... */
    setobj2s(L, L->top + 1, &v);  /* ... and its argument */
    L->top += 2;  /* and (next line) call the finalizer */
    L->ci->callstatus |= CIST_FIN;  /* will run a finalizer */
    status = luaD_pcall(L, dothecall, NULL, savestack(L, L->top - 2), 0);
    L->ci->callstatus &= ~CIST_FIN;  /* not running a finalizer anymore */
    L->allowhook = oldah;  /* restore hooks */
    g->gcrunning = running;  /* restore state */
    if (status != LUA_OK && propagateerrors) {  /* error while running __gc? */
      if (status == LUA_ERRRUN) {  /* is there an error object? */
        const char *msg = (ttisstring(s2v(L->top - 1)))
                            ? svalue(s2v(L->top - 1))
                            : "no message";
        luaO_pushfstring(L, "error in __gc metamethod (%s)", msg);
        status = LUA_ERRGCMM;  /* error in __gc metamethod */
      }
      luaD_throw(L, status);  /* re-throw error */
    }
  }
}


/*
** Call a few finalizers
*/
static int runafewfinalizers (lua_State *L, int n) {
  global_State *g = G(L);
  int i;
  for (i = 0; i < n && g->tobefnz; i++)
    GCTM(L, 1);  /* call one finalizer */
  return i;
}


/*
** call all pending finalizers
*/
static void callallpendingfinalizers (lua_State *L) {
  global_State *g = G(L);
  while (g->tobefnz)
    GCTM(L, 0);
}


/*
** find last 'next' field in list 'p' list (to add elements in its end)
*/
static GCObject **findlast (GCObject **p) {
  while (*p != NULL)
    p = &(*p)->next;
  return p;
}


/*
** Move all unreachable objects (or 'all' objects) that need
** finalization from list 'finobj' to list 'tobefnz' (to be finalized).
** (Note that objects after 'finobjold' cannot be white, so they
** don't need to be traversed. In incremental mode, 'finobjold' is NULL,
** so the whole list is traversed.)
*/
static void separatetobefnz (global_State *g, int all) {
  GCObject *curr;
  GCObject **p = &g->finobj;
  GCObject **lastnext = findlast(&g->tobefnz);
  while ((curr = *p) != g->finobjold) {  /* traverse all finalizable objects */
    lua_assert(tofinalize(curr));
    if (!(iswhite(curr) || all))  /* not being collected? */
      p = &curr->next;  /* don't bother with it */
    else {
      if (curr == g->finobjsur)  /* removing 'finobjsur'? */
        g->finobjsur = curr->next;  /* correct it */
      *p = curr->next;  /* remove 'curr' from 'finobj' list */
      curr->next = *lastnext;  /* link at the end of 'tobefnz' list */
      *lastnext = curr;
      lastnext = &curr->next;
    }
  }
}


/*
** if object 'o' has a finalizer, remove it from 'allgc' list (must
** search the list to find it) and link it in 'finobj' list.
*/
void luaC_checkfinalizer (lua_State *L, GCObject *o, Table *mt) {
  global_State *g = G(L);
  if (tofinalize(o) ||                 /* obj. is already marked... */
      gfasttm(g, mt, TM_GC) == NULL)   /* or has no finalizer? */
    return;  /* nothing to be done */
  else {  /* move 'o' to 'finobj' list */
    GCObject **p;
    if (issweepphase(g)) {
      makewhite(g, o);  /* "sweep" object 'o' */
      if (g->sweepgc == &o->next)  /* should not remove 'sweepgc' object */
        g->sweepgc = sweeptolive(L, g->sweepgc);  /* change 'sweepgc' */
    }
    else {  /* correct pointers into 'allgc' list, if needed */
      if (o == g->survival)
        g->survival = o->next;
      if (o == g->old)
        g->old = o->next;
      if (o == g->reallyold)
        g->reallyold = o->next;
    }
    /* search for pointer pointing to 'o' */
    for (p = &g->allgc; *p != o; p = &(*p)->next) { /* empty */ }
    *p = o->next;  /* remove 'o' from 'allgc' list */
    o->next = g->finobj;  /* link it in 'finobj' list */
    g->finobj = o;
    l_setbit(o->marked, FINALIZEDBIT);  /* mark it as such */
  }
}

/* }====================================================== */


/*
** {======================================================
** Generational Collector
** =======================================================
*/


/* mask to erase all color bits, not changing gen-related stuff */
#define maskgencolors	(~(bitmask(BLACKBIT) | WHITEBITS))


/*
** Sweep a list of objects, deleting dead ones and turning
** the non dead to old (without changing their colors).
*/
static void sweep2old (lua_State *L, GCObject **p) {
  GCObject *curr;
  while ((curr = *p) != NULL) {
    if (iswhite(curr)) {  /* is 'curr' dead? */
      lua_assert(isdead(G(L), curr));
      *p = curr->next;  /* remove 'curr' from list */
      freeobj(L, curr);  /* erase 'curr' */
    }
    else {  /* all surviving objects become old */
      setage(curr, G_OLD);
      p = &curr->next;  /* go to next element */
    }
  }
}


/*
** Sweep for generational mode. Delete dead objects. (Because the
** collection is not incremental, there are no "new white" objects
** during the sweep. So, any white object must be dead.) For
** non-dead objects, advance their ages and clear the color of
** new objects. (Old objects keep their colors.)
*/
static GCObject **sweepgen (lua_State *L, global_State *g, GCObject **p,
                            GCObject *limit) {
  static lu_byte nextage[] = {
    G_SURVIVAL,  /* from G_NEW */
    G_OLD1,      /* from G_SURVIVAL */
    G_OLD1,      /* from G_OLD0 */
    G_OLD,       /* from G_OLD1 */
    G_OLD,       /* from G_OLD (do not change) */
    G_TOUCHED1,  /* from G_TOUCHED1 (do not change) */
    G_TOUCHED2   /* from G_TOUCHED2 (do not change) */
  };
  int white = luaC_white(g);
  GCObject *curr;
  while ((curr = *p) != limit) {
    if (iswhite(curr)) {  /* is 'curr' dead? */
      lua_assert(!isold(curr) && isdead(g, curr));
      *p = curr->next;  /* remove 'curr' from list */
      freeobj(L, curr);  /* erase 'curr' */
    }
    else {  /* correct mark and age */
      if (getage(curr) == G_NEW)
        curr->marked = cast_byte((curr->marked & maskgencolors) | white);
      setage(curr, nextage[getage(curr)]);
      p = &curr->next;  /* go to next element */
    }
  }
  return p;
}


/*
** Traverse a list making all its elements white and clearing their
** age.
*/
static void whitelist (global_State *g, GCObject *p) {
  int white = luaC_white(g);
  for (; p != NULL; p = p->next)
    p->marked = cast_byte((p->marked & maskcolors) | white);
}


/*
** Correct a list of gray objects. Because this correction is
** done after sweeping, young objects can be white and still
** be in the list. They are only removed.
** For tables, advance 'touched1' to 'touched2'; 'touched2' objects
** become regular old and are removed from the list.
** For threads, just remove white ones from the list.
*/
static GCObject **correctgraylist (GCObject **p) {
  GCObject *curr;
  while ((curr = *p) != NULL) {
    switch (curr->tt) {
      case LUA_TTABLE: {
        Table *h = gco2t(curr);
        if (getage(h) == G_TOUCHED1) {  /* touched in this cycle? */
          lua_assert(isgray(h));
          gray2black(h);  /* make it black, for next barrier */
          changeage(h, G_TOUCHED1, G_TOUCHED2);
          p = &h->gclist;  /* go to next element */
        }
        else {
          if (!iswhite(h)) {
            lua_assert(isold(h));
            if (getage(h) == G_TOUCHED2)
              changeage(h, G_TOUCHED2, G_OLD);
            gray2black(h);  /* make it black */
          }
          *p = h->gclist;  /* remove 'curr' from gray list */
        }
        break;
      }
      case LUA_TTHREAD: {
        lua_State *th = gco2th(curr);
        lua_assert(!isblack(th));
        if (iswhite(th))  /* new object? */
          *p = th->gclist;  /* remove from gray list */
        else  /* old threads remain gray */
          p = &th->gclist;  /* go to next element */
        break;
      }
      default: lua_assert(0);  /* nothing more could be gray here */
    }
  }
  return p;
}


/*
** Correct all gray lists, coalescing them into 'grayagain'.
*/
static void correctgraylists (global_State *g) {
  GCObject **list = correctgraylist(&g->grayagain);
  *list = g->weak; g->weak = NULL;
  list = correctgraylist(list);
  *list = g->allweak; g->allweak = NULL;
  list = correctgraylist(list);
  *list = g->ephemeron; g->ephemeron = NULL;
  correctgraylist(list);
}


/*
** Mark 'old1' objects when starting a new young collection.
** Gray objects are already in some gray list, and so will be visited
** in the atomic step.
*/
static void markold (global_State *g, GCObject *from, GCObject *to) {
  GCObject *p;
  for (p = from; p != to; p = p->next) {
    if (getage(p) == G_OLD1) {
      lua_assert(!iswhite(p));
      if (isblack(p)) {
        black2gray(p);  /* should be '2white', but gray works too */
        reallymarkobject(g, p);
      }
    }
  }
}


/*
** Finish a young-generation collection.
*/
static void finishgencycle (lua_State *L, global_State *g) {
  correctgraylists(g);
  checkSizes(L, g);
  g->gcstate = GCSpropagate;  /* skip restart */
  callallpendingfinalizers(L);
}


/*
** Does a young collection. First, mark 'old1' objects.  (Only survival
** and "recent old" lists can contain 'old1' objects. New lists cannot
** contain 'old1' objects, at most 'old0' objects that were already
** visited when marked old.) Then does the atomic step. Then,
** sweep all lists and advance pointers. Finally, finish the collection.
*/
static void youngcollection (lua_State *L, global_State *g) {
  GCObject **psurvival;  /* to point to first non-dead survival object */
  lua_assert(g->gcstate == GCSpropagate);
  markold(g, g->survival, g->reallyold);
  markold(g, g->finobj, g->finobjrold);
  atomic(L);

  /* sweep nursery and get a pointer to its last live element */
  psurvival = sweepgen(L, g, &g->allgc, g->survival);
  /* sweep 'survival' and 'old' */
  sweepgen(L, g, psurvival, g->reallyold);
  g->reallyold = g->old;
  g->old = *psurvival;  /* 'survival' survivals are old now */
  g->survival = g->allgc;  /* all news are survivals */

  /* repeat for 'finobj' lists */
  psurvival = sweepgen(L, g, &g->finobj, g->finobjsur);
  /* sweep 'survival' and 'old' */
  sweepgen(L, g, psurvival, g->finobjrold);
  g->finobjrold = g->finobjold;
  g->finobjold = *psurvival;  /* 'survival' survivals are old now */
  g->finobjsur = g->finobj;  /* all news are survivals */

  sweepgen(L, g, &g->tobefnz, NULL);

  finishgencycle(L, g);
}


/*
** Enter generational mode. Must go until the end of an atomic cycle
** to ensure that all threads are in the gray list. Then, turn all
** objects into old and finishes the collection.
*/
static void entergen (lua_State *L, global_State *g) {
  luaC_runtilstate(L, bitmask(GCSpause));  /* prepare to start a new cycle */
  luaC_runtilstate(L, bitmask(GCSpropagate));  /* start new cycle */
  atomic(L);
  /* sweep all elements making them old */
  sweep2old(L, &g->allgc);
  /* everything alive now is old */
  g->reallyold = g->old = g->survival = g->allgc;

  /* repeat for 'finobj' lists */
  sweep2old(L, &g->finobj);
  g->finobjrold = g->finobjold = g->finobjsur = g->finobj;

  sweep2old(L, &g->tobefnz);

  finishgencycle(L, g);
  g->gckind = KGC_GEN;
  g->GCestimate = gettotalbytes(g);  /* base for memory control */
}


/*
** Enter incremental mode. Turn all objects white, make all
** intermediate lists point to NULL (to avoid invalid pointers),
** and go to pause state.
*/
static void enterinc (global_State *g) {
  whitelist(g, g->allgc);
  g->reallyold = g->old = g->survival = NULL;
  whitelist(g, g->finobj);
  g->finobjrold = g->finobjold = g->finobjsur = NULL;
  lua_assert(g->tobefnz == NULL);  /* no need to sweep */
  g->gcstate = GCSpause;
  g->gckind = KGC_INC;
}


/*
** Change collector mode to 'newmode'.
*/
void luaC_changemode (lua_State *L, int newmode) {
  global_State *g = G(L);
  if (newmode != g->gckind) {
    if (newmode == KGC_GEN)  /* entering generational mode? */
      entergen(L, g);
    else
      enterinc(g);  /* entering incremental mode */
  }
}


/*
** Does a full collection in generational mode.
*/
static void fullgen (lua_State *L, global_State *g) {
  enterinc(g);
  entergen(L, g);
}


/*
** Does a generational "step". If memory grows 'genmajormul'% larger
** than last major collection (kept in 'g->GCestimate'), does a major
** collection. Otherwise, does a minor collection and set debt to make
** another collection when memory grows 'genminormul'% larger.
** 'GCdebt <= 0' means an explicit call to GC step with "size" zero;
** in that case, always do a minor collection.
*/
static void genstep (lua_State *L, global_State *g) {
  lu_mem majorbase = g->GCestimate;
  int majormul = getgcparam(g->genmajormul);
  if (g->GCdebt > 0 &&
      gettotalbytes(g) > (majorbase / 100) * (100 + majormul)) {
    fullgen(L, g);
  }
  else {
    lu_mem mem;
    youngcollection(L, g);
    mem = gettotalbytes(g);
    luaE_setdebt(g, -(cast(l_mem, (mem / 100)) * g->genminormul));
    g->GCestimate = majorbase;  /* preserve base value */
  }
}

/* }====================================================== */


/*
** {======================================================
** GC control
** =======================================================
*/


/*
** Set the "time" to wait before starting a new GC cycle; cycle will
** start when memory use hits the threshold of ('estimate' * pause /
** PAUSEADJ). (Division by 'estimate' should be OK: it cannot be zero,
** because Lua cannot even start with less than PAUSEADJ bytes).
*/
static void setpause (global_State *g) {
  l_mem threshold, debt;
  int pause = getgcparam(g->gcpause);
  l_mem estimate = g->GCestimate / PAUSEADJ;  /* adjust 'estimate' */
  lua_assert(estimate > 0);
  threshold = (pause < MAX_LMEM / estimate)  /* overflow? */
            ? estimate * pause  /* no overflow */
            : MAX_LMEM;  /* overflow; truncate to maximum */
  debt = gettotalbytes(g) - threshold;
  if (debt > 0) debt = 0;
  luaE_setdebt(g, debt);
}


/*
** Enter first sweep phase.
** The call to 'sweeptolive' makes the pointer point to an object
** inside the list (instead of to the header), so that the real sweep do
** not need to skip objects created between "now" and the start of the
** real sweep.
*/
static void entersweep (lua_State *L) {
  global_State *g = G(L);
  g->gcstate = GCSswpallgc;
  lua_assert(g->sweepgc == NULL);
  g->sweepgc = sweeptolive(L, &g->allgc);
}


/*
** Delete all objects in list 'p' until (but not including) object
** 'limit'.
*/
static void deletelist (lua_State *L, GCObject *p, GCObject *limit) {
  while (p != limit) {
    GCObject *next = p->next;
    freeobj(L, p);
    p = next;
  }
}


/*
** Call all finalizers of the objects in the given Lua state, and
** then free all objects, except for the main thread.
*/
void luaC_freeallobjects (lua_State *L) {
  global_State *g = G(L);
  luaC_changemode(L, KGC_INC);
  separatetobefnz(g, 1);  /* separate all objects with finalizers */
  lua_assert(g->finobj == NULL);
  callallpendingfinalizers(L);
  deletelist(L, g->allgc, obj2gco(g->mainthread));
  deletelist(L, g->finobj, NULL);
  deletelist(L, g->fixedgc, NULL);  /* collect fixed objects */
  lua_assert(g->strt.nuse == 0);
}


static lu_mem atomic (lua_State *L) {
  global_State *g = G(L);
  lu_mem work = 0;
  GCObject *origweak, *origall;
  GCObject *grayagain = g->grayagain;  /* save original list */
  g->grayagain = NULL;
  lua_assert(g->ephemeron == NULL && g->weak == NULL);
  lua_assert(!iswhite(g->mainthread));
  g->gcstate = GCSatomic;
  markobject(g, L);  /* mark running thread */
  /* registry and global metatables may be changed by API */
  markvalue(g, &g->l_registry);
  markmt(g);  /* mark global metatables */
  /* remark occasional upvalues of (maybe) dead threads */
  work += remarkupvals(g);
  work += propagateall(g);  /* propagate changes */
  g->gray = grayagain;
  work += propagateall(g);  /* traverse 'grayagain' list */
  convergeephemerons(g);
  /* at this point, all strongly accessible objects are marked. */
  /* Clear values from weak tables, before checking finalizers */
  clearvalues(g, g->weak, NULL);
  clearvalues(g, g->allweak, NULL);
  origweak = g->weak; origall = g->allweak;
  separatetobefnz(g, 0);  /* separate objects to be finalized */
  work += markbeingfnz(g);  /* mark objects that will be finalized */
  work += propagateall(g);  /* remark, to propagate 'resurrection' */
  convergeephemerons(g);
  /* at this point, all resurrected objects are marked. */
  /* remove dead objects from weak tables */
  clearkeys(g, g->ephemeron);  /* clear keys from all ephemeron tables */
  clearkeys(g, g->allweak);  /* clear keys from all 'allweak' tables */
  /* clear values from resurrected weak tables */
  clearvalues(g, g->weak, origweak);
  clearvalues(g, g->allweak, origall);
  luaS_clearcache(g);
  clearprotolist(g);
  g->currentwhite = cast_byte(otherwhite(g));  /* flip current white */
  lua_assert(g->gray == NULL);
  return work;  /* estimate of slots marked by 'atomic' */
}


static int sweepstep (lua_State *L, global_State *g,
                      int nextstate, GCObject **nextlist) {
  if (g->sweepgc) {
    l_mem olddebt = g->GCdebt;
    int count;
    g->sweepgc = sweeplist(L, g->sweepgc, GCSWEEPMAX, &count);
    g->GCestimate += g->GCdebt - olddebt;  /* update estimate */
    return count;
  }
  else {  /* enter next state */
    g->gcstate = nextstate;
    g->sweepgc = nextlist;
    return 0;  /* no work done */
  }
}


static lu_mem singlestep (lua_State *L) {
  global_State *g = G(L);
  switch (g->gcstate) {
    case GCSpause: {
      restartcollection(g);
      g->gcstate = GCSpropagate;
      return 1;
    }
    case GCSpropagate: {
      if (g->gray == NULL) {  /* no more gray objects? */
        g->gcstate = GCSenteratomic;  /* finish propagate phase */
        return 0;
      }
      else
        return propagatemark(g);  /* traverse one gray object */
    }
    case GCSenteratomic: {
      lu_mem work = propagateall(g);  /* make sure gray list is empty */
      work += atomic(L);  /* work is what was traversed by 'atomic' */
      entersweep(L);
      g->GCestimate = gettotalbytes(g);  /* first estimate */;
      return work;
    }
    case GCSswpallgc: {  /* sweep "regular" objects */
      return sweepstep(L, g, GCSswpfinobj, &g->finobj);
    }
    case GCSswpfinobj: {  /* sweep objects with finalizers */
      return sweepstep(L, g, GCSswptobefnz, &g->tobefnz);
    }
    case GCSswptobefnz: {  /* sweep objects to be finalized */
      return sweepstep(L, g, GCSswpend, NULL);
    }
    case GCSswpend: {  /* finish sweeps */
      checkSizes(L, g);
      g->gcstate = GCScallfin;
      return 0;
    }
    case GCScallfin: {  /* call remaining finalizers */
      if (g->tobefnz && !g->gcemergency) {
        int n = runafewfinalizers(L, GCFINMAX);
        return n * GCFINALIZECOST;
      }
      else {  /* emergency mode or no more finalizers */
        g->gcstate = GCSpause;  /* finish collection */
        return 0;
      }
    }
    default: lua_assert(0); return 0;
  }
}


/*
** advances the garbage collector until it reaches a state allowed
** by 'statemask'
*/
void luaC_runtilstate (lua_State *L, int statesmask) {
  global_State *g = G(L);
  while (!testbit(statesmask, g->gcstate))
    singlestep(L);
}


/*
** Performs a basic incremental step. The debt and step size are
** converted from bytes to "units of work"; then the function loops
** running single steps until adding that many units of work or
** finishing a cycle (pause state). Finally, it sets the debt that
** controls when next step will be performed.
*/
static void incstep (lua_State *L, global_State *g) {
  int stepmul = (getgcparam(g->gcstepmul) | 1);  /* avoid division by 0 */
  l_mem debt = (g->GCdebt / WORK2MEM) * stepmul;
  l_mem stepsize = (g->gcstepsize <= log2maxs(l_mem))
                 ? ((cast(l_mem, 1) << g->gcstepsize) / WORK2MEM) * stepmul
                 : MAX_LMEM;  /* overflow; keep maximum value */
  do {  /* repeat until pause or enough "credit" (negative debt) */
    lu_mem work = singlestep(L);  /* perform one single step */
    debt -= work;
  } while (debt > -stepsize && g->gcstate != GCSpause);
  if (g->gcstate == GCSpause)
    setpause(g);  /* pause until next cycle */
  else {
    debt = (debt / stepmul) * WORK2MEM;  /* convert 'work units' to bytes */
    luaE_setdebt(g, debt);
  }
}

/*
** performs a basic GC step if collector is running
*/
void luaC_step (lua_State *L) {
  global_State *g = G(L);
  if (g->gcrunning) {  /* running? */
    if (g->gckind == KGC_INC)
      incstep(L, g);
    else
      genstep(L, g);
  }
}


/*
** Perform a full collection in incremental mode.
** Before running the collection, check 'keepinvariant'; if it is true,
** there may be some objects marked as black, so the collector has
** to sweep all objects to turn them back to white (as white has not
** changed, nothing will be collected).
*/
static void fullinc (lua_State *L, global_State *g) {
  if (keepinvariant(g))  /* black objects? */
    entersweep(L); /* sweep everything to turn them back to white */
  /* finish any pending sweep phase to start a new cycle */
  luaC_runtilstate(L, bitmask(GCSpause));
  luaC_runtilstate(L, bitmask(GCScallfin));  /* run up to finalizers */
  /* estimate must be correct after a full GC cycle */
  lua_assert(g->GCestimate == gettotalbytes(g));
  luaC_runtilstate(L, bitmask(GCSpause));  /* finish collection */
  setpause(g);
}


/*
** Performs a full GC cycle; if 'isemergency', set a flag to avoid
** some operations which could change the interpreter state in some
** unexpected ways (running finalizers and shrinking some structures).
*/
void luaC_fullgc (lua_State *L, int isemergency) {
  global_State *g = G(L);
  lua_assert(!g->gcemergency);
  g->gcemergency = isemergency;  /* set flag */
  if (g->gckind == KGC_INC)
    fullinc(L, g);
  else
    fullgen(L, g);
  g->gcemergency = 0;
}

/* }====================================================== */


