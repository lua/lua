/*
** $Id: lgc.c,v 2.103 2010/10/25 19:01:37 roberto Exp $
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



/* how much to allocate before next GC step */
#define GCSTEPSIZE	1024

/* maximum number of elements to sweep in each single step */
#define GCSWEEPMAX	40

/* cost of sweeping one element */
#define GCSWEEPCOST	1

/* maximum number of finalizers to call in each GC step */
#define GCFINALIZENUM	4

/* cost of marking the root set */
#define GCROOTCOST	10

/* cost of atomic step */
#define GCATOMICCOST	1000

/* basic cost to traverse one object (to be added to the links the
   object may have) */
#define TRAVCOST	5


/*
** standard negative debt for GC; a reasonable "time" to wait before
** starting a new cycle
*/
#define stddebt(g)	(-cast(l_mem, g->totalbytes/100) * g->gcpause)


/*
** 'makewhite' erases all color bits plus the old bit and then
** sets only the current white bit
*/
#define maskcolors	(~(bit2mask(BLACKBIT, OLDBIT) | WHITEBITS))
#define makewhite(g,x)	\
 (gch(x)->marked = cast_byte((gch(x)->marked & maskcolors) | luaC_white(g)))

#define white2gray(x)	resetbits(gch(x)->marked, WHITEBITS)
#define black2gray(x)	resetbit(gch(x)->marked, BLACKBIT)

#define stringmark(s)	((void)((s) && resetbits((s)->tsv.marked, WHITEBITS)))


#define isfinalized(u)		testbit((u)->marked, FINALIZEDBIT)

#define checkdeadkey(n)	lua_assert(!ttisdeadkey(gkey(n)) || ttisnil(gval(n)))


#define markvalue(g,o) { checkconsistency(o); \
  if (valiswhite(o)) reallymarkobject(g,gcvalue(o)); }

#define markobject(g,t) { if ((t) && iswhite(obj2gco(t))) \
		reallymarkobject(g, obj2gco(t)); }

static void reallymarkobject (global_State *g, GCObject *o);


/*
** {======================================================
** Generic functions
** =======================================================
*/


/*
** link table 'h' into list pointed by 'p'
*/
#define linktable(h,p)	((h)->gclist = *(p), *(p) = obj2gco(h))


/*
** mark a table entry as dead (therefore removing it from the table)
*/
static void removeentry (Node *n) {
  lua_assert(ttisnil(gval(n)));
  if (iscollectable(gkey(n)))
    setdeadvalue(gkey(n));  /* dead key; remove it */
}


/*
** tells whether a key or value can be cleared from a weak
** table. Non-collectable objects are never removed from weak
** tables. Strings behave as `values', so are never removed too. for
** other objects: if really collected, cannot keep them; for objects
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


/*
** barrier that moves collector forward, that is, mark the white object
** being pointed by a black object.
*/
void luaC_barrier_ (lua_State *L, GCObject *o, GCObject *v) {
  global_State *g = G(L);
  lua_assert(isblack(o) && iswhite(v) && !isdead(g, v) && !isdead(g, o));
  lua_assert(isgenerational(g) || g->gcstate != GCSpause);
  lua_assert(gch(o)->tt != LUA_TTABLE);
  if (keepinvariant(g))  /* must keep invariant? */
    reallymarkobject(g, v);  /* restore invariant */
  else {  /* sweep phase */
    lua_assert(issweepphase(g));
    makewhite(g, o);  /* mark main obj. as white to avoid other barriers */
  }
}


/*
** barrier that moves collector backward, that is, mark the black object
** pointing to a white object as gray again. (Current implementation
** only works for tables; access to 'gclist' is not uniform across
** different types.)
*/
void luaC_barrierback_ (lua_State *L, GCObject *o) {
  global_State *g = G(L);
  lua_assert(isblack(o) && !isdead(g, o));
  black2gray(o);  /* make object gray (again) */
  gco2t(o)->gclist = g->grayagain;
  g->grayagain = o;
}


/*
** barrier for prototypes. When creating first closure (cache is
** NULL), use a forward barrier; this may be the only closure of the
** prototype (if it is a "regular" function, with a single instance)
** and the prototype may be big, so it is better to avoid traversing
** it again. Otherwise, use a backward barrier, to avoid marking all
** possible instances.
*/
LUAI_FUNC void luaC_barrierproto_ (lua_State *L, Proto *p, Closure *c) {
  global_State *g = G(L);
  lua_assert(isblack(obj2gco(p)));
  if (p->cache == NULL) {  /* first time? */
    luaC_objbarrier(L, p, c);
  }
  else {  /* use a backward barrier */
    black2gray(obj2gco(p));  /* make prototype gray (again) */
    p->gclist = g->grayagain;
    g->grayagain = obj2gco(p);
  }
}


/*
** check color (and invariants) for an upvalue that was closed,
** i.e., moved into the 'allgc' list
*/
void luaC_checkupvalcolor (global_State *g, UpVal *uv) {
  GCObject *o = obj2gco(uv);
  lua_assert(!isblack(o));  /* open upvalues are never black */
  if (isgray(o)) {
    if (keepinvariant(g)) {
      resetoldbit(o);  /* see MOVE OLD rule */
      gray2black(o);  /* it is being visited now */
      markvalue(g, uv->v);
    }
    else {
      lua_assert(issweepphase(g));
      makewhite(g, o);
    }
  }
}


/*
** create a new collectable object (with given type and size) and link
** it to '*list'. 'offset' tells how many bytes to allocate before the
** object itself (used only by states).
*/
GCObject *luaC_newobj (lua_State *L, int tt, size_t sz, GCObject **list,
                       int offset) {
  global_State *g = G(L);
  GCObject *o = obj2gco(cast(char *, luaM_newobject(L, tt, sz)) + offset);
  if (list == NULL)
    list = &g->allgc;  /* standard list for collectable objects */
  gch(o)->marked = luaC_white(g);
  gch(o)->tt = tt;
  gch(o)->next = *list;
  *list = o;
  return o;
}

/* }====================================================== */



/*
** {======================================================
** Mark functions
** =======================================================
*/


/*
** mark an object. Userdata and closed upvalues are visited and turned
** black here. Strings remain gray (it is the same as making them
** black). Other objects are marked gray and added to appropriate list
** to be visited (and turned black) later. (Open upvalues are already
** linked in 'headuv' list.)
*/
static void reallymarkobject (global_State *g, GCObject *o) {
  lua_assert(iswhite(o) && !isdead(g, o));
  white2gray(o);
  switch (gch(o)->tt) {
    case LUA_TSTRING: {
      return;  /* for strings, gray is as good as black */
    }
    case LUA_TUSERDATA: {
      Table *mt = gco2u(o)->metatable;
      markobject(g, mt);
      markobject(g, gco2u(o)->env);
      gray2black(o);  /* all pointers marked */
      return;
    }
    case LUA_TUPVAL: {
      UpVal *uv = gco2uv(o);
      markvalue(g, uv->v);
      if (uv->v == &uv->u.value)  /* closed? (open upvalues remain gray) */
        gray2black(o);  /* make it black */
      return;
    }
    case LUA_TFUNCTION: {
      gco2cl(o)->c.gclist = g->gray;
      g->gray = o;
      break;
    }
    case LUA_TTABLE: {
      linktable(gco2t(o), &g->gray);
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


/*
** mark tag methods for basic types
*/
static void markmt (global_State *g) {
  int i;
  for (i=0; i < LUA_NUMTAGS; i++)
    markobject(g, g->mt[i]);
}


/*
** mark all objects in list of being-finalized
*/
static void markbeingfnz (global_State *g) {
  GCObject *o;
  for (o = g->tobefnz; o != NULL; o = gch(o)->next) {
    makewhite(g, o);
    reallymarkobject(g, o);
  }
}


/*
** mark all values stored in marked open upvalues. (See comment in
** 'lstate.h'.)
*/
static void remarkupvals (global_State *g) {
  UpVal *uv;
  for (uv = g->uvhead.u.l.next; uv != &g->uvhead; uv = uv->u.l.next) {
    if (isgray(obj2gco(uv)))
      markvalue(g, uv->v);
  }
}


/*
** mark root set and reset all gray lists, to start a new
** incremental (or full) collection
*/
static void markroot (lua_State *L) {
  global_State *g = G(L);
  g->gray = g->grayagain = NULL;
  g->weak = g->allweak = g->ephemeron = NULL;
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

static void traverseweakvalue (global_State *g, Table *h) {
  Node *n, *limit = gnode(h, sizenode(h));
  for (n = gnode(h, 0); n < limit; n++) {
    checkdeadkey(n);
    if (ttisnil(gval(n)))  /* entry is empty? */
      removeentry(n);  /* remove it */
    else {
      lua_assert(!ttisnil(gkey(n)));
      markvalue(g, gkey(n));  /* mark key */
    }
  }
  linktable(h, &g->weak);  /* link into appropriate list */
}


static int traverseephemeron (global_State *g, Table *h) {
  int marked = 0;  /* true if an object is marked in this traversal */
  int hasclears = 0;  /* true if table has unmarked pairs */
  Node *n, *limit = gnode(h, sizenode(h));
  int i;
  /* traverse array part (numeric keys are 'strong') */
  for (i = 0; i < h->sizearray; i++) {
    if (valiswhite(&h->array[i])) {
      marked = 1;
      reallymarkobject(g, gcvalue(&h->array[i]));
    }
  }
  /* traverse hash part */
  for (n = gnode(h, 0); n < limit; n++) {
    checkdeadkey(n);
    if (ttisnil(gval(n)))  /* entry is empty? */
      removeentry(n);  /* remove it */
    else if (valiswhite(gval(n))) {  /* value not marked yet? */
      if (iscleared(gkey(n), 1))  /* key is not marked (yet)? */
        hasclears = 1;  /* may have to propagate mark from key to value */
      else {  /* key is marked, so mark value */
        marked = 1;  /* value was not marked */
        reallymarkobject(g, gcvalue(gval(n)));
      }
    }
  }
  if (hasclears)  /* does table have unmarked pairs? */
    linktable(h, &g->ephemeron);  /* will have to propagate again */
  else  /* nothing to propagate */
    linktable(h, &g->weak);  /* avoid convergence phase  */
  return marked;
}


static void traversestrongtable (global_State *g, Table *h) {
  Node *n, *limit = gnode(h, sizenode(h));
  int i;
  for (i = 0; i < h->sizearray; i++)  /* traverse array part */
    markvalue(g, &h->array[i]);
  for (n = gnode(h, 0); n < limit; n++) {  /* traverse hash part */
    checkdeadkey(n);
    if (ttisnil(gval(n)))  /* entry is empty? */
      removeentry(n);  /* remove it */
    else {
      lua_assert(!ttisnil(gkey(n)));
      markvalue(g, gkey(n));  /* mark key */
      markvalue(g, gval(n));  /* mark value */
    }
  }
}


static int traversetable (global_State *g, Table *h) {
  const TValue *mode = gfasttm(g, h->metatable, TM_MODE);
  markobject(g, h->metatable);
  if (mode && ttisstring(mode)) {  /* is there a weak mode? */
    int weakkey = (strchr(svalue(mode), 'k') != NULL);
    int weakvalue = (strchr(svalue(mode), 'v') != NULL);
    if (weakkey || weakvalue) {  /* is really weak? */
      black2gray(obj2gco(h));  /* keep table gray */
      if (!weakkey) {  /* strong keys? */
        traverseweakvalue(g, h);
        return TRAVCOST + sizenode(h);
      }
      else if (!weakvalue) {  /* strong values? */
        traverseephemeron(g, h);
        return TRAVCOST + h->sizearray + sizenode(h);
      }
      else {
        linktable(h, &g->allweak);  /* nothing to traverse now */
        return TRAVCOST;
      }
    }  /* else go through */
  }
  traversestrongtable(g, h);
  return TRAVCOST + h->sizearray + (2 * sizenode(h));
}


static int traverseproto (global_State *g, Proto *f) {
  int i;
  if (f->cache && iswhite(obj2gco(f->cache)))
    f->cache = NULL;  /* allow cache to be collected */
  stringmark(f->source);
  for (i = 0; i < f->sizek; i++)  /* mark literals */
    markvalue(g, &f->k[i]);
  for (i = 0; i < f->sizeupvalues; i++)  /* mark upvalue names */
    stringmark(f->upvalues[i].name);
  for (i = 0; i < f->sizep; i++)  /* mark nested protos */
    markobject(g, f->p[i]);
  for (i = 0; i < f->sizelocvars; i++)  /* mark local-variable names */
    stringmark(f->locvars[i].varname);
  return TRAVCOST + f->sizek + f->sizeupvalues + f->sizep + f->sizelocvars;
}


static int traverseclosure (global_State *g, Closure *cl) {
  if (cl->c.isC) {
    int i;
    for (i=0; i<cl->c.nupvalues; i++)  /* mark its upvalues */
      markvalue(g, &cl->c.upvalue[i]);
  }
  else {
    int i;
    lua_assert(cl->l.nupvalues == cl->l.p->sizeupvalues);
    markobject(g, cl->l.p);  /* mark its prototype */
    for (i=0; i<cl->l.nupvalues; i++)  /* mark its upvalues */
      markobject(g, cl->l.upvals[i]);
  }
  return TRAVCOST + cl->c.nupvalues;
}


static int traversestack (global_State *g, lua_State *L) {
  StkId o = L->stack;
  if (o == NULL)
    return 1;  /* stack not completely built yet */
  for (; o < L->top; o++)
    markvalue(g, o);
  if (g->gcstate == GCSatomic) {  /* final traversal? */
    StkId lim = L->stack + L->stacksize;  /* real end of stack */
    for (; o < lim; o++)  /* clear not-marked stack slice */
      setnilvalue(o);
  }
  return TRAVCOST + cast_int(o - L->stack);
}


/*
** traverse one gray object, turning it to black (except for threads,
** which are always gray).
** Returns number of values traversed.
*/
static int propagatemark (global_State *g) {
  GCObject *o = g->gray;
  lua_assert(isgray(o));
  gray2black(o);
  switch (gch(o)->tt) {
    case LUA_TTABLE: {
      Table *h = gco2t(o);
      g->gray = h->gclist;
      return traversetable(g, h);
    }
    case LUA_TFUNCTION: {
      Closure *cl = gco2cl(o);
      g->gray = cl->c.gclist;
      return traverseclosure(g, cl);
    }
    case LUA_TTHREAD: {
      lua_State *th = gco2th(o);
      g->gray = th->gclist;
      th->gclist = g->grayagain;
      g->grayagain = o;
      black2gray(o);
      return traversestack(g, th);
    }
    case LUA_TPROTO: {
      Proto *p = gco2p(o);
      g->gray = p->gclist;
      return traverseproto(g, p);
    }
    default: lua_assert(0); return 0;
  }
}


static void propagateall (global_State *g) {
  while (g->gray) propagatemark(g);
}


static void traverselistofgrays (global_State *g, GCObject **l) {
  lua_assert(g->gray == NULL);  /* no grays left */
  g->gray = *l;  /* now 'l' is new gray list */
  *l = NULL;
  propagateall(g);
}


static void convergeephemerons (global_State *g) {
  int changed;
  do {
    GCObject *w;
    GCObject *next = g->ephemeron;  /* get ephemeron list */
    g->ephemeron = NULL;  /* tables will return to this list when traversed */
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


/*
** clear collected entries from all weaktables in list 'l'
*/
static void cleartable (GCObject *l) {
  for (; l != NULL; l = gco2t(l)->gclist) {
    Table *h = gco2t(l);
    Node *n, *limit = gnode(h, sizenode(h));
    int i;
    for (i = 0; i < h->sizearray; i++) {
      TValue *o = &h->array[i];
      if (iscleared(o, 0))  /* value was collected? */
        setnilvalue(o);  /* remove value */
    }
    for (n = gnode(h, 0); n < limit; n++) {
      if (!ttisnil(gval(n)) &&  /* non-empty entry? */
          (iscleared(gkey(n), 1) || iscleared(gval(n), 0))) {
        setnilvalue(gval(n));  /* remove value ... */
        removeentry(n);  /* and remove entry from table */
      }
    }
  }
}


static void freeobj (lua_State *L, GCObject *o) {
  switch (gch(o)->tt) {
    case LUA_TPROTO: luaF_freeproto(L, gco2p(o)); break;
    case LUA_TFUNCTION: luaF_freeclosure(L, gco2cl(o)); break;
    case LUA_TUPVAL: luaF_freeupval(L, gco2uv(o)); break;
    case LUA_TTABLE: luaH_free(L, gco2t(o)); break;
    case LUA_TTHREAD: luaE_freethread(L, gco2th(o)); break;
    case LUA_TUSERDATA: luaM_freemem(L, o, sizeudata(gco2u(o))); break;
    case LUA_TSTRING: {
      G(L)->strt.nuse--;
      luaM_freemem(L, o, sizestring(gco2ts(o)));
      break;
    }
    default: lua_assert(0);
  }
}


#define sweepwholelist(L,p)	sweeplist(L,p,MAX_LUMEM)
static GCObject **sweeplist (lua_State *L, GCObject **p, lu_mem count);


/*
** sweep the (open) upvalues of a thread and resize its stack and
** list of call-info structures.
*/
static void sweepthread (lua_State *L, lua_State *L1) {
  if (L1->stack == NULL) return;  /* stack not completely built yet */
  sweepwholelist(L, &L1->openupval);  /* sweep open upvalues */
  luaE_freeCI(L1);  /* free extra CallInfo slots */
  /* should not change the stack during an emergency gc cycle */
  if (G(L)->gckind != KGC_EMERGENCY)
    luaD_shrinkstack(L1);
}


/*
** sweep at most 'count' elements from a list of GCObjects erasing dead
** objects, where a dead (not alive) object is one marked with the "old"
** (non current) white and not fixed.
** In non-generational mode, change all non-dead objects back to white,
** preparing for next collection cycle.
** In generational mode, keep black objects black, and also mark them as
** old; stop when hitting an old object, as all objects after that
** one will be old too.
** When object is a thread, sweep its list of open upvalues too.
*/
static GCObject **sweeplist (lua_State *L, GCObject **p, lu_mem count) {
  global_State *g = G(L);
  int ow = otherwhite(g);
  int toclear, toset;  /* bits to clear and to set in all live objects */
  int tostop;  /* stop sweep when this is true */
  if (isgenerational(g)) {  /* generational mode? */
    toclear = ~0;  /* clear nothing */
    toset = bitmask(OLDBIT);  /* set the old bit of all surviving objects */
    tostop = bitmask(OLDBIT);  /* do not sweep old generation */
  }
  else {  /* normal mode */
    toclear = maskcolors;  /* clear all color bits + old bit */
    toset = luaC_white(g);  /* make object white */
    tostop = 0;  /* do not stop */
  }
  while (*p != NULL && count-- > 0) {
    GCObject *curr = *p;
    int marked = gch(curr)->marked;
    if (isdeadm(ow, marked)) {  /* is 'curr' dead? */
      *p = gch(curr)->next;  /* remove 'curr' from list */
      freeobj(L, curr);  /* erase 'curr' */
    }
    else {
      if (gch(curr)->tt == LUA_TTHREAD)
        sweepthread(L, gco2th(curr));  /* sweep thread's upvalues */
      if (testbits(marked, tostop)) {
        static GCObject *nullp = NULL;
        return &nullp;  /* stop sweeping this list */
      }
      /* update marks */
      gch(curr)->marked = cast_byte((marked & toclear) | toset);
      p = &gch(curr)->next;  /* go to next element */
    }
  }
  return p;
}

/* }====================================================== */


/*
** {======================================================
** Finalization
** =======================================================
*/

static void checkSizes (lua_State *L) {
  global_State *g = G(L);
  if (g->gckind != KGC_EMERGENCY) {  /* do not change sizes in emergency */
    int hs = g->strt.size / 2;  /* half the size of the string table */
    if (g->strt.nuse < cast(lu_int32, hs))  /* using less than that half? */
      luaS_resize(L, hs);  /* halve its size */
    luaZ_freebuffer(L, &g->buff);  /* free concatenation buffer */
  }
}


static Udata *udata2finalize (global_State *g) {
  GCObject *o = g->tobefnz;  /* get first element */
  Udata *u = rawgco2u(o);
  lua_assert(isfinalized(&u->uv));
  lua_assert(!isold(o));
  g->tobefnz = u->uv.next;  /* remove it from 'tobefnz' list */
  u->uv.next = g->allgc;  /* return it to 'allgc' list */
  g->allgc = o;
  resetbit(u->uv.marked, SEPARATED);  /* mark that it is not in 'tobefnz' */
  resetoldbit(o); /* see MOVE OLD rule */
  if (!keepinvariant(g))  /* not keeping invariant? */
    makewhite(g, o);  /* "sweep" object */
  return u;
}


static void dothecall (lua_State *L, void *ud) {
  UNUSED(ud);
  luaD_call(L, L->top - 2, 0, 0);
}


static void GCTM (lua_State *L, int propagateerrors) {
  global_State *g = G(L);
  Udata *udata = udata2finalize(g);
  const TValue *tm = gfasttm(g, udata->uv.metatable, TM_GC);
  if (tm != NULL && ttisfunction(tm)) {  /* is there a finalizer? */
    int status;
    lu_byte oldah = L->allowhook;
    lu_mem oldd = g->GCdebt;
    L->allowhook = 0;  /* stop debug hooks during GC tag method */
    stopgc(g);  /* avoid GC steps */
    setobj2s(L, L->top, tm);  /* push finalizer... */
    setuvalue(L, L->top+1, udata);  /* ... and its argument */
    L->top += 2;  /* and (next line) call the finalizer */
    status = luaD_pcall(L, dothecall, NULL, savestack(L, L->top - 2), 0);
    L->allowhook = oldah;  /* restore hooks */
    g->GCdebt = oldd;  /* restore threshold */
    if (status != LUA_OK && propagateerrors) {  /* error while running __gc? */
      if (status == LUA_ERRRUN) {  /* is there an error msg.? */
        luaO_pushfstring(L, "error in __gc tag method (%s)",
                                        lua_tostring(L, -1));
        status = LUA_ERRGCMM;  /* error in __gc metamethod */
      }
      luaD_throw(L, status);  /* re-send error */
    }
  }
}


/*
** move all unreachable udata that need finalization from list 'udgc' to
** list 'tobefnz'
*/
void luaC_separateudata (lua_State *L, int all) {
  global_State *g = G(L);
  GCObject **p = &g->udgc;
  GCObject *curr;
  GCObject **lastnext = &g->tobefnz;
  /* find last 'next' field in 'tobefnz' list (to add elements in its end) */
  while (*lastnext != NULL)
    lastnext = &gch(*lastnext)->next;
  while ((curr = *p) != NULL) {  /* traverse all finalizable objects */
    lua_assert(gch(curr)->tt == LUA_TUSERDATA && !isfinalized(gco2u(curr)));
    lua_assert(testbit(gch(curr)->marked, SEPARATED));
    if (!(all || iswhite(curr)))  /* not being collected? */
      p = &gch(curr)->next;  /* don't bother with it */
    else {
      l_setbit(gch(curr)->marked, FINALIZEDBIT); /* won't be finalized again */
      *p = gch(curr)->next;  /* remove 'curr' from 'udgc' list */
      gch(curr)->next = *lastnext;  /* link at the end of 'tobefnz' list */
      *lastnext = curr;
      lastnext = &gch(curr)->next;
    }
  }
}


/*
** if userdata 'u' has a finalizer, remove it from 'allgc' list (must
** search the list to find it) and link it in 'udgc' list.
*/
void luaC_checkfinalizer (lua_State *L, Udata *u) {
  global_State *g = G(L);
  if (testbit(u->uv.marked, SEPARATED) || /* udata is already separated... */
      isfinalized(&u->uv) ||                     /* ... or is finalized... */
      gfasttm(g, u->uv.metatable, TM_GC) == NULL)  /* or has no finalizer? */
    return;  /* nothing to be done */
  else {  /* move 'u' to 'udgc' list */
    GCObject **p;
    for (p = &g->allgc; *p != obj2gco(u); p = &gch(*p)->next) ;
    *p = u->uv.next;  /* remove 'u' from root list */
    u->uv.next = g->udgc;  /* link it in list 'udgc' */
    g->udgc = obj2gco(u);
    l_setbit(u->uv.marked, SEPARATED);  /* mark it as such */
    resetoldbit(obj2gco(u));  /* see MOVE OLD rule */
  }
}

/* }====================================================== */


/*
** {======================================================
** GC control
** =======================================================
*/


#define sweepphases  \
	(bitmask(GCSsweepstring) | bitmask(GCSsweepudata) | bitmask(GCSsweep))

/*
** change GC mode
*/
void luaC_changemode (lua_State *L, int mode) {
  global_State *g = G(L);
  if (mode == g->gckind) return;  /* nothing to change */
  if (mode == KGC_GEN) {  /* change to generational mode */
    /* make sure gray lists are consistent */
    luaC_runtilstate(L, bitmask(GCSpropagate));
    g->lastmajormem = g->totalbytes;
    g->gckind = KGC_GEN;
  }
  else {  /* change to incremental mode */
    /* sweep all objects to turn them back to white
       (as white has not changed, nothing extra will be collected) */
    g->sweepstrgc = 0;
    g->gcstate = GCSsweepstring;
    g->gckind = KGC_NORMAL;
    luaC_runtilstate(L, ~sweepphases);
  }
}


/*
** call all pending finalizers */
static void callallpendingfinalizers (lua_State *L, int propagateerrors) {
  global_State *g = G(L);
  while (g->tobefnz) GCTM(L, propagateerrors);
}


void luaC_freeallobjects (lua_State *L) {
  global_State *g = G(L);
  int i;
  callallpendingfinalizers(L, 0);
  /* following "white" makes all objects look dead */
  g->currentwhite = WHITEBITS;
  g->gckind = KGC_NORMAL;
  sweepwholelist(L, &g->udgc);
  lua_assert(g->udgc == NULL);
  sweepwholelist(L, &g->allgc);
  lua_assert(g->allgc == NULL);
  for (i = 0; i < g->strt.size; i++)  /* free all string lists */
    sweepwholelist(L, &g->strt.hash[i]);
  lua_assert(g->strt.nuse == 0);
}


static void atomic (lua_State *L) {
  global_State *g = G(L);
  lua_assert(!iswhite(obj2gco(g->mainthread)));
  markobject(g, L);  /* mark running thread */
  /* registry and global metatables may be changed by API */
  markvalue(g, &g->l_registry);
  markmt(g);  /* mark basic metatables */
  /* remark occasional upvalues of (maybe) dead threads */
  remarkupvals(g);
  /* traverse objects caught by write barrier and by 'remarkupvals' */
  propagateall(g);
  traverselistofgrays(g, &g->weak);  /* remark weak tables */
  traverselistofgrays(g, &g->ephemeron);  /* remark ephemeron tables */
  traverselistofgrays(g, &g->grayagain);  /* remark gray again */
  convergeephemerons(g);
  /* at this point, all strongly accessible objects are marked. */
  luaC_separateudata(L, 0);  /* separate userdata to be finalized */
  markbeingfnz(g);  /* mark userdata that will be finalized */
  propagateall(g);  /* remark, to propagate `preserveness' */
  convergeephemerons(g);
  /* remove collected objects from weak tables */
  cleartable(g->weak);
  cleartable(g->ephemeron);
  cleartable(g->allweak);
  g->sweepstrgc = 0;  /* prepare to sweep strings */
  g->gcstate = GCSsweepstring;
  g->currentwhite = cast_byte(otherwhite(g));  /* flip current white */
  /*lua_checkmemory(L);*/
}


static l_mem singlestep (lua_State *L) {
  global_State *g = G(L);
  switch (g->gcstate) {
    case GCSpause: {
      if (!isgenerational(g))
        markroot(L);  /* start a new collection */
      /* in any case, root must be marked */
      lua_assert(!iswhite(obj2gco(g->mainthread))
              && !iswhite(gcvalue(&g->l_registry)));
      g->gcstate = GCSpropagate;
      return GCROOTCOST;
    }
    case GCSpropagate: {
      if (g->gray)
        return propagatemark(g);
      else {  /* no more `gray' objects */
        g->gcstate = GCSatomic;  /* finish mark phase */
        atomic(L);
        return GCATOMICCOST;
      }
    }
    case GCSsweepstring: {
      if (g->sweepstrgc < g->strt.size) {
        sweepwholelist(L, &g->strt.hash[g->sweepstrgc++]);
        return GCSWEEPCOST;
      }
      else {  /* no more strings to sweep */
        g->sweepgc = &g->udgc;  /* prepare to sweep userdata */
        g->gcstate = GCSsweepudata;
        return 0;
      }
    }
    case GCSsweepudata: {
      if (*g->sweepgc) {
        g->sweepgc = sweeplist(L, g->sweepgc, GCSWEEPMAX);
        return GCSWEEPMAX*GCSWEEPCOST;
      }
      else {
        g->sweepgc = &g->allgc;  /* go to next phase */
        g->gcstate = GCSsweep;
        return GCSWEEPCOST;
      }
    }
    case GCSsweep: {
      if (*g->sweepgc) {
        g->sweepgc = sweeplist(L, g->sweepgc, GCSWEEPMAX);
        return GCSWEEPMAX*GCSWEEPCOST;
      }
      else {
        /* sweep main thread */
        GCObject *mt = obj2gco(g->mainthread);
        sweeplist(L, &mt, 1);
        checkSizes(L);
        g->gcstate = GCSpause;  /* finish collection */
        return GCSWEEPCOST;
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


static void generationalcollection (lua_State *L) {
  global_State *g = G(L);
  if (g->lastmajormem == 0) {  /* signal for another major collection? */
    luaC_fullgc(L, 0);  /* perform a full regular collection */
    g->lastmajormem = g->totalbytes;  /* update control */
  }
  else {
    luaC_runtilstate(L, ~bitmask(GCSpause));  /* run complete cycle */
    luaC_runtilstate(L, bitmask(GCSpause));
    if (g->totalbytes > g->lastmajormem/100 * g->gcmajorinc)
      g->lastmajormem = 0;  /* signal for a major collection */
  }
  g->GCdebt = stddebt(g);
}


static void step (lua_State *L) {
  global_State *g = G(L);
  l_mem lim = g->gcstepmul;  /* how much to work */
  do {  /* always perform at least one single step */
    lim -= singlestep(L);
  } while (lim > 0 && g->gcstate != GCSpause);
  if (g->gcstate != GCSpause)
    g->GCdebt -= GCSTEPSIZE;
  else
    g->GCdebt = stddebt(g);
}


void luaC_step (lua_State *L) {
  int i;
  if (isgenerational(G(L))) generationalcollection(L);
  else step(L);
  for (i = 0; i < GCFINALIZENUM && G(L)->tobefnz; i++)
    GCTM(L, 1);  /* Call a few pending finalizers */
}


/*
** performs a full GC cycle; if "isemergency", does not call
** finalizers (which could change stack positions)
*/
void luaC_fullgc (lua_State *L, int isemergency) {
  global_State *g = G(L);
  int origkind = g->gckind;
  lua_assert(origkind != KGC_EMERGENCY);
  if (!isemergency)   /* do not run finalizers during emergency GC */
    callallpendingfinalizers(L, 1);
  if (keepinvariant(g)) {  /* marking phase? */
    /* must sweep all objects to turn them back to white
       (as white has not changed, nothing will be collected) */
    g->sweepstrgc = 0;
    g->gcstate = GCSsweepstring;
  }
  g->gckind = isemergency ? KGC_EMERGENCY : KGC_NORMAL;
  /* finish any pending sweep phase to start a new cycle */
  luaC_runtilstate(L, bitmask(GCSpause));
  /* run entire collector */
  luaC_runtilstate(L, ~bitmask(GCSpause));
  luaC_runtilstate(L, bitmask(GCSpause));
  if (origkind == KGC_GEN) {  /* generational mode? */
    /* generational mode must always start in propagate phase */
    luaC_runtilstate(L, bitmask(GCSpropagate));
  }
  g->gckind = origkind;
  g->GCdebt = stddebt(g);
  if (!isemergency)   /* do not run finalizers during emergency GC */
    callallpendingfinalizers(L, 1);
}

/* }====================================================== */


