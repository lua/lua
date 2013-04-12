/*
** $Id: ltests.c,v 2.135 2013/03/16 21:10:18 roberto Exp $
** Internal Module for Debugging of the Lua Implementation
** See Copyright Notice in lua.h
*/


#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ltests_c
#define LUA_CORE

#include "lua.h"

#include "lapi.h"
#include "lauxlib.h"
#include "lcode.h"
#include "lctype.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lmem.h"
#include "lopcodes.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "lualib.h"



/*
** The whole module only makes sense with LUA_DEBUG on
*/
#if defined(LUA_DEBUG)


void *l_Trick = 0;


int islocked = 0;


#define obj_at(L,k)	(L->ci->func + (k))


static void setnameval (lua_State *L, const char *name, int val) {
  lua_pushstring(L, name);
  lua_pushinteger(L, val);
  lua_settable(L, -3);
}


static void pushobject (lua_State *L, const TValue *o) {
  setobj2s(L, L->top, o);
  api_incr_top(L);
}


static int tpanic (lua_State *L) {
  fprintf(stderr, "PANIC: unprotected error in call to Lua API (%s)\n",
                   lua_tostring(L, -1));
  return (exit(EXIT_FAILURE), 0);  /* do not return to Lua */
}


/*
** {======================================================================
** Controlled version for realloc.
** =======================================================================
*/

#define MARK		0x55  /* 01010101 (a nice pattern) */

typedef union Header {
  L_Umaxalign a;  /* ensures maximum alignment for Header */
  struct {
    size_t size;
    int type;
  } d;
} Header;


#if !defined(EXTERNMEMCHECK)

/* full memory check */
#define MARKSIZE	16  /* size of marks after each block */
#define fillmem(mem,size)	memset(mem, -MARK, size)

#else

/* external memory check: don't do it twice */
#define MARKSIZE	0
#define fillmem(mem,size)	/* empty */

#endif


Memcontrol l_memcontrol =
  {0L, 0L, 0L, 0L, {0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L}};


static void freeblock (Memcontrol *mc, Header *block) {
  if (block) {
    size_t size = block->d.size;
    int i;
    for (i = 0; i < MARKSIZE; i++)  /* check marks after block */
      lua_assert(*(cast(char *, block + 1) + size + i) == MARK);
    mc->objcount[block->d.type]--;
    fillmem(block, sizeof(Header) + size + MARKSIZE);  /* erase block */
    free(block);  /* actually free block */
    mc->numblocks--;  /* update counts */
    mc->total -= size;
  }
}


void *debug_realloc (void *ud, void *b, size_t oldsize, size_t size) {
  Memcontrol *mc = cast(Memcontrol *, ud);
  Header *block = cast(Header *, b);
  int type;
  if (mc->memlimit == 0) {  /* first time? */
    char *limit = getenv("MEMLIMIT");  /* initialize memory limit */
    mc->memlimit = limit ? strtoul(limit, NULL, 10) : ULONG_MAX;
  }
  if (block == NULL) {
    type = (oldsize < LUA_NUMTAGS) ? oldsize : 0;
    oldsize = 0;
  }
  else {
    block--;  /* go to real header */
    type = block->d.type;
    lua_assert(oldsize == block->d.size);
  }
  if (size == 0) {
    freeblock(mc, block);
    return NULL;
  }
  else if (size > oldsize && mc->total+size-oldsize > mc->memlimit)
    return NULL;  /* fake a memory allocation error */
  else {
    Header *newblock;
    int i;
    size_t commonsize = (oldsize < size) ? oldsize : size;
    size_t realsize = sizeof(Header) + size + MARKSIZE;
    if (realsize < size) return NULL;  /* arithmetic overflow! */
    newblock = cast(Header *, malloc(realsize));  /* alloc a new block */
    if (newblock == NULL) return NULL;  /* really out of memory? */
    if (block) {
      memcpy(newblock + 1, block + 1, commonsize);  /* copy old contents */
      freeblock(mc, block);  /* erase (and check) old copy */
    }
    /* initialize new part of the block with something `weird' */
    fillmem(cast(char *, newblock + 1) + commonsize, size - commonsize);
    /* initialize marks after block */
    for (i = 0; i < MARKSIZE; i++)
      *(cast(char *, newblock + 1) + size + i) = MARK;
    newblock->d.size = size;
    newblock->d.type = type;
    mc->total += size;
    if (mc->total > mc->maxmem)
      mc->maxmem = mc->total;
    mc->numblocks++;
    mc->objcount[type]++;
    return newblock + 1;
  }
}


/* }====================================================================== */



/*
** {======================================================
** Functions to check memory consistency
** =======================================================
*/


static int testobjref1 (global_State *g, GCObject *f, GCObject *t) {
  if (isdead(g,t)) return 0;
  if (!issweepphase(g))
    return !(isblack(f) && iswhite(t));
  else return 1;
}


static void printobj (global_State *g, GCObject *o) {
  int i = 1;
  GCObject *p;
  for (p = g->allgc; p != o && p != NULL; p = gch(p)->next) i++;
  if (p == NULL) {
    i = 1;
    for (p = g->finobj; p != o && p != NULL; p = gch(p)->next) i++;
    if (p == NULL) i = 0;  /* zero means 'not found' */
    else i = -i;  /* negative means 'found in findobj list */
  }
  printf("||%d:%s(%p)-%c(%02X)||", i, ttypename(gch(o)->tt), (void *)o,
           isdead(g,o)?'d':isblack(o)?'b':iswhite(o)?'w':'g', gch(o)->marked);
}


static int testobjref (global_State *g, GCObject *f, GCObject *t) {
  int r = testobjref1(g,f,t);
  if (!r) {
    printf("%d(%02X) - ", g->gcstate, g->currentwhite);
    printobj(g, f);
    printf("\t-> ");
    printobj(g, t);
    printf("\n");
  }
  return r;
}

#define checkobjref(g,f,t) lua_assert(testobjref(g,f,obj2gco(t)))


static void checkvalref (global_State *g, GCObject *f, const TValue *t) {
  if (iscollectable(t)) {
    lua_assert(righttt(t));
    lua_assert(testobjref(g, f, gcvalue(t)));
  }
}


static void checktable (global_State *g, Table *h) {
  int i;
  Node *n, *limit = gnode(h, sizenode(h));
  GCObject *hgc = obj2gco(h);
  if (h->metatable)
    checkobjref(g, hgc, h->metatable);
  for (i = 0; i < h->sizearray; i++)
    checkvalref(g, hgc, &h->array[i]);
  for (n = gnode(h, 0); n < limit; n++) {
    if (!ttisnil(gval(n))) {
      lua_assert(!ttisnil(gkey(n)));
      checkvalref(g, hgc, gkey(n));
      checkvalref(g, hgc, gval(n));
    }
  }
}


/*
** All marks are conditional because a GC may happen while the
** prototype is still being created
*/
static void checkproto (global_State *g, Proto *f) {
  int i;
  GCObject *fgc = obj2gco(f);
  if (f->source) checkobjref(g, fgc, f->source);
  for (i=0; i<f->sizek; i++) {
    if (ttisstring(f->k+i))
      checkobjref(g, fgc, rawtsvalue(f->k+i));
  }
  for (i=0; i<f->sizeupvalues; i++) {
    if (f->upvalues[i].name)
      checkobjref(g, fgc, f->upvalues[i].name);
  }
  for (i=0; i<f->sizep; i++) {
    if (f->p[i])
      checkobjref(g, fgc, f->p[i]);
  }
  for (i=0; i<f->sizelocvars; i++) {
    if (f->locvars[i].varname)
      checkobjref(g, fgc, f->locvars[i].varname);
  }
}



static void checkCclosure (global_State *g, CClosure *cl) {
  GCObject *clgc = obj2gco(cl);
  int i;
  for (i = 0; i < cl->nupvalues; i++)
    checkvalref(g, clgc, &cl->upvalue[i]);
}


static void checkLclosure (global_State *g, LClosure *cl) {
  GCObject *clgc = obj2gco(cl);
  int i;
  if (cl->p) checkobjref(g, clgc, cl->p);
  for (i=0; i<cl->nupvalues; i++) {
    if (cl->upvals[i]) {
      lua_assert(cl->upvals[i]->tt == LUA_TUPVAL);
      checkobjref(g, clgc, cl->upvals[i]);
    }
  }
}


static int lua_checkpc (pCallInfo ci) {
  if (!isLua(ci)) return 1;
  else {
    Proto *p = ci_func(ci)->p;
    return p->code <= ci->u.l.savedpc &&
           ci->u.l.savedpc <= p->code + p->sizecode;
  }
}


static void checkstack (global_State *g, lua_State *L1) {
  StkId o;
  CallInfo *ci;
  GCObject *uvo;
  lua_assert(!isdead(g, obj2gco(L1)));
  for (uvo = L1->openupval; uvo != NULL; uvo = gch(uvo)->next) {
    UpVal *uv = gco2uv(uvo);
    lua_assert(uv->v != &uv->u.value);  /* must be open */
    lua_assert(!isblack(uvo));  /* open upvalues cannot be black */
  }
  for (ci = L1->ci; ci != NULL; ci = ci->previous) {
    lua_assert(ci->top <= L1->stack_last);
    lua_assert(lua_checkpc(ci));
  }
  if (L1->stack) {
    for (o = L1->stack; o < L1->top; o++)
      checkliveness(g, o);
  }
  else lua_assert(L1->stacksize == 0);
}


static void checkobject (global_State *g, GCObject *o, int maybedead) {
  if (isdead(g, o))
    lua_assert(maybedead);
  else {
    if (g->gcstate == GCSpause)
      lua_assert(iswhite(o));
    switch (gch(o)->tt) {
      case LUA_TUPVAL: {
        UpVal *uv = gco2uv(o);
        lua_assert(uv->v == &uv->u.value);  /* must be closed */
        lua_assert(!isgray(o));  /* closed upvalues are never gray */
        checkvalref(g, o, uv->v);
        break;
      }
      case LUA_TUSERDATA: {
        Table *mt = gco2u(o)->metatable;
        if (mt) checkobjref(g, o, mt);
        break;
      }
      case LUA_TTABLE: {
        checktable(g, gco2t(o));
        break;
      }
      case LUA_TTHREAD: {
        checkstack(g, gco2th(o));
        break;
      }
      case LUA_TLCL: {
        checkLclosure(g, gco2lcl(o));
        break;
      }
      case LUA_TCCL: {
        checkCclosure(g, gco2ccl(o));
        break;
      }
      case LUA_TPROTO: {
        checkproto(g, gco2p(o));
        break;
      }
      case LUA_TSHRSTR:
      case LUA_TLNGSTR: break;
      default: lua_assert(0);
    }
  }
}


#define TESTGRAYBIT		7

static void checkgraylist (GCObject *l) {
  while (l) {
    lua_assert(isgray(l));
    lua_assert(!testbit(l->gch.marked, TESTGRAYBIT));
    l_setbit(l->gch.marked, TESTGRAYBIT);
    switch (gch(l)->tt) {
      case LUA_TTABLE: l = gco2t(l)->gclist; break;
      case LUA_TLCL: l = gco2lcl(l)->gclist; break;
      case LUA_TCCL: l = gco2ccl(l)->gclist; break;
      case LUA_TTHREAD: l = gco2th(l)->gclist; break;
      case LUA_TPROTO: l = gco2p(l)->gclist; break;
      default: lua_assert(0);  /* other objects cannot be gray */
    }
  }
}


/*
** mark all objects in gray lists with the TESTGRAYBIT, so that
** 'checkmemory' can check that all gray objects are in a gray list
*/
static void markgrays (global_State *g) {
  if (!keepinvariant(g)) return;
  checkgraylist(g->gray);
  checkgraylist(g->grayagain);
  checkgraylist(g->weak);
  checkgraylist(g->ephemeron);
  checkgraylist(g->allweak);
}


static void checkold (global_State *g, GCObject *o) {
  int isold = 0;
  for (; o != NULL; o = gch(o)->next) {
    if (isold(o)) {  /* old generation? */
      lua_assert(isgenerational(g));
      if (!issweepphase(g))
        isold = 1;
    }
    else lua_assert(!isold);  /* non-old object cannot be after an old one */
    if (isgray(o)) {
      lua_assert(!keepinvariant(g) || testbit(o->gch.marked, TESTGRAYBIT));
      resetbit(o->gch.marked, TESTGRAYBIT);
    }
    lua_assert(!testbit(o->gch.marked, TESTGRAYBIT));
  }
}


int lua_checkmemory (lua_State *L) {
  global_State *g = G(L);
  GCObject *o;
  UpVal *uv;
  int maybedead;
  if (keepinvariant(g)) {
    lua_assert(!iswhite(obj2gco(g->mainthread)));
    lua_assert(!iswhite(gcvalue(&g->l_registry)));
  }
  else  /* generational mode keeps collector in 'propagate' state */
    lua_assert(!isgenerational(g));
  lua_assert(!isdead(g, gcvalue(&g->l_registry)));
  checkstack(g, g->mainthread);
  resetbit(g->mainthread->marked, TESTGRAYBIT);
  /* check 'allgc' list */
  markgrays(g);
  checkold(g, g->allgc);
  lua_assert(g->sweepgc == NULL || issweepphase(g));
  maybedead = 0;
  for (o = g->allgc; o != NULL; o = gch(o)->next) {
    if (g->sweepgc && o == *g->sweepgc)
      maybedead = 1;  /* part of the list not yet swept */
    checkobject(g, o, maybedead);
    lua_assert(!testbit(o->gch.marked, SEPARATED));
  }
  /* check 'finobj' list */
  checkold(g, g->finobj);
  for (o = g->finobj; o != NULL; o = gch(o)->next) {
    lua_assert(testbit(o->gch.marked, SEPARATED));
    lua_assert(gch(o)->tt == LUA_TUSERDATA ||
               gch(o)->tt == LUA_TTABLE);
    checkobject(g, o, 0);
  }
  /* check 'tobefnz' list */
  checkold(g, g->tobefnz);
  for (o = g->tobefnz; o != NULL; o = gch(o)->next) {
    lua_assert(!iswhite(o) || g->gcstate == GCSpause);
    lua_assert(!isdead(g, o) && testbit(o->gch.marked, SEPARATED));
    lua_assert(gch(o)->tt == LUA_TUSERDATA ||
               gch(o)->tt == LUA_TTABLE);
  }
  /* check 'uvhead' list */
  for (uv = g->uvhead.u.l.next; uv != &g->uvhead; uv = uv->u.l.next) {
    lua_assert(uv->u.l.next->u.l.prev == uv && uv->u.l.prev->u.l.next == uv);
    lua_assert(uv->v != &uv->u.value);  /* must be open */
    lua_assert(!isblack(obj2gco(uv)));  /* open upvalues are never black */
    if (!isdead(g, obj2gco(uv)))
      checkvalref(g, obj2gco(uv), uv->v);
  }
  return 0;
}

/* }====================================================== */



/*
** {======================================================
** Disassembler
** =======================================================
*/


static char *buildop (Proto *p, int pc, char *buff) {
  Instruction i = p->code[pc];
  OpCode o = GET_OPCODE(i);
  const char *name = luaP_opnames[o];
  int line = getfuncline(p, pc);
  sprintf(buff, "(%4d) %4d - ", line, pc);
  switch (getOpMode(o)) {
    case iABC:
      sprintf(buff+strlen(buff), "%-12s%4d %4d %4d", name,
              GETARG_A(i), GETARG_B(i), GETARG_C(i));
      break;
    case iABx:
      sprintf(buff+strlen(buff), "%-12s%4d %4d", name, GETARG_A(i), GETARG_Bx(i));
      break;
    case iAsBx:
      sprintf(buff+strlen(buff), "%-12s%4d %4d", name, GETARG_A(i), GETARG_sBx(i));
      break;
    case iAx:
      sprintf(buff+strlen(buff), "%-12s%4d", name, GETARG_Ax(i));
      break;
  }
  return buff;
}


#if 0
void luaI_printcode (Proto *pt, int size) {
  int pc;
  for (pc=0; pc<size; pc++) {
    char buff[100];
    printf("%s\n", buildop(pt, pc, buff));
  }
  printf("-------\n");
}


void luaI_printinst (Proto *pt, int pc) {
  char buff[100];
  printf("%s\n", buildop(pt, pc, buff));
}
#endif


static int listcode (lua_State *L) {
  int pc;
  Proto *p;
  luaL_argcheck(L, lua_isfunction(L, 1) && !lua_iscfunction(L, 1),
                 1, "Lua function expected");
  p = getproto(obj_at(L, 1));
  lua_newtable(L);
  setnameval(L, "maxstack", p->maxstacksize);
  setnameval(L, "numparams", p->numparams);
  for (pc=0; pc<p->sizecode; pc++) {
    char buff[100];
    lua_pushinteger(L, pc+1);
    lua_pushstring(L, buildop(p, pc, buff));
    lua_settable(L, -3);
  }
  return 1;
}


static int listk (lua_State *L) {
  Proto *p;
  int i;
  luaL_argcheck(L, lua_isfunction(L, 1) && !lua_iscfunction(L, 1),
                 1, "Lua function expected");
  p = getproto(obj_at(L, 1));
  lua_createtable(L, p->sizek, 0);
  for (i=0; i<p->sizek; i++) {
    pushobject(L, p->k+i);
    lua_rawseti(L, -2, i+1);
  }
  return 1;
}


static int listlocals (lua_State *L) {
  Proto *p;
  int pc = luaL_checkint(L, 2) - 1;
  int i = 0;
  const char *name;
  luaL_argcheck(L, lua_isfunction(L, 1) && !lua_iscfunction(L, 1),
                 1, "Lua function expected");
  p = getproto(obj_at(L, 1));
  while ((name = luaF_getlocalname(p, ++i, pc)) != NULL)
    lua_pushstring(L, name);
  return i-1;
}

/* }====================================================== */




static int get_limits (lua_State *L) {
  lua_createtable(L, 0, 5);
  setnameval(L, "BITS_INT", LUAI_BITSINT);
  setnameval(L, "LFPF", LFIELDS_PER_FLUSH);
  setnameval(L, "MAXSTACK", MAXSTACK);
  setnameval(L, "NUM_OPCODES", NUM_OPCODES);
  return 1;
}


static int mem_query (lua_State *L) {
  if (lua_isnone(L, 1)) {
    lua_pushinteger(L, l_memcontrol.total);
    lua_pushinteger(L, l_memcontrol.numblocks);
    lua_pushinteger(L, l_memcontrol.maxmem);
    return 3;
  }
  else if (lua_isnumber(L, 1)) {
    l_memcontrol.memlimit = luaL_checkint(L, 1);
    return 0;
  }
  else {
    const char *t = luaL_checkstring(L, 1);
    int i;
    for (i = LUA_NUMTAGS - 1; i >= 0; i--) {
      if (strcmp(t, ttypename(i)) == 0) {
        lua_pushinteger(L, l_memcontrol.objcount[i]);
        return 1;
      }
    }
    return luaL_error(L, "unkown type '%s'", t);
  }
}


static int settrick (lua_State *L) {
  if (ttisnil(obj_at(L, 1)))
    l_Trick = NULL;
  else
    l_Trick = gcvalue(obj_at(L, 1));
  return 0;
}


static int get_gccolor (lua_State *L) {
  TValue *o;
  luaL_checkany(L, 1);
  o = obj_at(L, 1);
  if (!iscollectable(o))
    lua_pushstring(L, "no collectable");
  else {
    int marked = gcvalue(o)->gch.marked;
    int n = 1;
    lua_pushstring(L, iswhite(gcvalue(o)) ? "white" :
                      isblack(gcvalue(o)) ? "black" : "grey");
    if (testbit(marked, FINALIZEDBIT)) {
      lua_pushliteral(L, "/finalized"); n++;
    }
    if (testbit(marked, SEPARATED)) {
      lua_pushliteral(L, "/separated"); n++;
    }
    if (testbit(marked, FIXEDBIT)) {
      lua_pushliteral(L, "/fixed"); n++;
    }
    if (testbit(marked, OLDBIT)) {
      lua_pushliteral(L, "/old"); n++;
    }
    lua_concat(L, n);
  }
  return 1;
}


static int gc_state (lua_State *L) {
  static const char *statenames[] = {"propagate", "atomic",
    "sweepstring", "sweepudata", "sweep", "pause", ""};
  int option = luaL_checkoption(L, 1, "", statenames);
  if (option == GCSpause + 1) {
    lua_pushstring(L, statenames[G(L)->gcstate]);
    return 1;
  }
  else {
    global_State *g = G(L);
    if (g->gckind == KGC_GEN && option == GCSpause)
      luaL_error(L, "cannot go to 'pause' state in generational mode");
    lua_lock(L);
    if (option < g->gcstate) {  /* must cross 'pause'? */
      luaC_runtilstate(L, bitmask(GCSpause));  /* run until pause */
      if (g->gckind == KGC_GEN)
        g->gcstate = GCSpropagate;  /* skip pause in gen. mode */
    }
    luaC_runtilstate(L, bitmask(option));
    lua_assert(G(L)->gcstate == option);
    lua_unlock(L);
    return 0;
  }
}


static int hash_query (lua_State *L) {
  if (lua_isnone(L, 2)) {
    luaL_argcheck(L, lua_type(L, 1) == LUA_TSTRING, 1, "string expected");
    lua_pushinteger(L, tsvalue(obj_at(L, 1))->hash);
  }
  else {
    TValue *o = obj_at(L, 1);
    Table *t;
    luaL_checktype(L, 2, LUA_TTABLE);
    t = hvalue(obj_at(L, 2));
    lua_pushinteger(L, luaH_mainposition(t, o) - t->node);
  }
  return 1;
}


static int stacklevel (lua_State *L) {
  unsigned long a = 0;
  lua_pushinteger(L, (L->top - L->stack));
  lua_pushinteger(L, (L->stack_last - L->stack));
  lua_pushinteger(L, (unsigned long)&a);
  return 5;
}


static int table_query (lua_State *L) {
  const Table *t;
  int i = luaL_optint(L, 2, -1);
  luaL_checktype(L, 1, LUA_TTABLE);
  t = hvalue(obj_at(L, 1));
  if (i == -1) {
    lua_pushinteger(L, t->sizearray);
    lua_pushinteger(L, luaH_isdummy(t->node) ? 0 : sizenode(t));
    lua_pushinteger(L, t->lastfree - t->node);
  }
  else if (i < t->sizearray) {
    lua_pushinteger(L, i);
    pushobject(L, &t->array[i]);
    lua_pushnil(L);
  }
  else if ((i -= t->sizearray) < sizenode(t)) {
    if (!ttisnil(gval(gnode(t, i))) ||
        ttisnil(gkey(gnode(t, i))) ||
        ttisnumber(gkey(gnode(t, i)))) {
      pushobject(L, gkey(gnode(t, i)));
    }
    else
      lua_pushliteral(L, "<undef>");
    pushobject(L, gval(gnode(t, i)));
    if (gnext(&t->node[i]))
      lua_pushinteger(L, gnext(&t->node[i]) - t->node);
    else
      lua_pushnil(L);
  }
  return 3;
}


static int string_query (lua_State *L) {
  stringtable *tb = &G(L)->strt;
  int s = luaL_optint(L, 2, 0) - 1;
  if (s==-1) {
    lua_pushinteger(L ,tb->nuse);
    lua_pushinteger(L ,tb->size);
    return 2;
  }
  else if (s < tb->size) {
    GCObject *ts;
    int n = 0;
    for (ts = tb->hash[s]; ts; ts = gch(ts)->next) {
      setsvalue2s(L, L->top, rawgco2ts(ts));
      api_incr_top(L);
      n++;
    }
    return n;
  }
  return 0;
}


static int tref (lua_State *L) {
  int level = lua_gettop(L);
  luaL_checkany(L, 1);
  lua_pushvalue(L, 1);
  lua_pushinteger(L, luaL_ref(L, LUA_REGISTRYINDEX));
  lua_assert(lua_gettop(L) == level+1);  /* +1 for result */
  return 1;
}

static int getref (lua_State *L) {
  int level = lua_gettop(L);
  lua_rawgeti(L, LUA_REGISTRYINDEX, luaL_checkint(L, 1));
  lua_assert(lua_gettop(L) == level+1);
  return 1;
}

static int unref (lua_State *L) {
  int level = lua_gettop(L);
  luaL_unref(L, LUA_REGISTRYINDEX, luaL_checkint(L, 1));
  lua_assert(lua_gettop(L) == level);
  return 0;
}


static int upvalue (lua_State *L) {
  int n = luaL_checkint(L, 2);
  luaL_checktype(L, 1, LUA_TFUNCTION);
  if (lua_isnone(L, 3)) {
    const char *name = lua_getupvalue(L, 1, n);
    if (name == NULL) return 0;
    lua_pushstring(L, name);
    return 2;
  }
  else {
    const char *name = lua_setupvalue(L, 1, n);
    lua_pushstring(L, name);
    return 1;
  }
}


static int newuserdata (lua_State *L) {
  size_t size = luaL_checkint(L, 1);
  char *p = cast(char *, lua_newuserdata(L, size));
  while (size--) *p++ = '\0';
  return 1;
}


static int pushuserdata (lua_State *L) {
  lua_pushlightuserdata(L, cast(void *, luaL_checkinteger(L, 1)));
  return 1;
}


static int udataval (lua_State *L) {
  lua_pushinteger(L, cast(long, lua_touserdata(L, 1)));
  return 1;
}


static int doonnewstack (lua_State *L) {
  lua_State *L1 = lua_newthread(L);
  size_t l;
  const char *s = luaL_checklstring(L, 1, &l);
  int status = luaL_loadbuffer(L1, s, l, s);
  if (status == LUA_OK)
    status = lua_pcall(L1, 0, 0, 0);
  lua_pushinteger(L, status);
  return 1;
}


static int s2d (lua_State *L) {
  lua_pushnumber(L, *cast(const double *, luaL_checkstring(L, 1)));
  return 1;
}


static int d2s (lua_State *L) {
  double d = luaL_checknumber(L, 1);
  lua_pushlstring(L, cast(char *, &d), sizeof(d));
  return 1;
}


static int num2int (lua_State *L) {
  lua_pushinteger(L, lua_tointeger(L, 1));
  return 1;
}


static int newstate (lua_State *L) {
  void *ud;
  lua_Alloc f = lua_getallocf(L, &ud);
  lua_State *L1 = lua_newstate(f, ud);
  if (L1) {
    lua_atpanic(L1, tpanic);
    lua_pushlightuserdata(L, L1);
  }
  else
    lua_pushnil(L);
  return 1;
}


static lua_State *getstate (lua_State *L) {
  lua_State *L1 = cast(lua_State *, lua_touserdata(L, 1));
  luaL_argcheck(L, L1 != NULL, 1, "state expected");
  return L1;
}


static int loadlib (lua_State *L) {
  static const luaL_Reg libs[] = {
    {"_G", luaopen_base},
    {"coroutine", luaopen_coroutine},
    {"debug", luaopen_debug},
    {"io", luaopen_io},
    {"os", luaopen_os},
    {"math", luaopen_math},
    {"string", luaopen_string},
    {"table", luaopen_table},
    {NULL, NULL}
  };
  lua_State *L1 = getstate(L);
  int i;
  luaL_requiref(L1, "package", luaopen_package, 1);
  luaL_getsubtable(L1, LUA_REGISTRYINDEX, "_PRELOAD");
  for (i = 0; libs[i].name; i++) {
    lua_pushcfunction(L1, libs[i].func);
    lua_setfield(L1, -2, libs[i].name);
  }
  return 0;
}

static int closestate (lua_State *L) {
  lua_State *L1 = getstate(L);
  lua_close(L1);
  return 0;
}

static int doremote (lua_State *L) {
  lua_State *L1 = getstate(L);
  size_t lcode;
  const char *code = luaL_checklstring(L, 2, &lcode);
  int status;
  lua_settop(L1, 0);
  status = luaL_loadbuffer(L1, code, lcode, code);
  if (status == LUA_OK)
    status = lua_pcall(L1, 0, LUA_MULTRET, 0);
  if (status != LUA_OK) {
    lua_pushnil(L);
    lua_pushstring(L, lua_tostring(L1, -1));
    lua_pushinteger(L, status);
    return 3;
  }
  else {
    int i = 0;
    while (!lua_isnone(L1, ++i))
      lua_pushstring(L, lua_tostring(L1, i));
    lua_pop(L1, i-1);
    return i-1;
  }
}


static int int2fb_aux (lua_State *L) {
  int b = luaO_int2fb(luaL_checkint(L, 1));
  lua_pushinteger(L, b);
  lua_pushinteger(L, luaO_fb2int(b));
  return 2;
}



/*
** {======================================================
** function to test the API with C. It interprets a kind of assembler
** language with calls to the API, so the test can be driven by Lua code
** =======================================================
*/


static void sethookaux (lua_State *L, int mask, int count, const char *code);

static const char *const delimits = " \t\n,;";

static void skip (const char **pc) {
  for (;;) {
    if (**pc != '\0' && strchr(delimits, **pc)) (*pc)++;
    else if (**pc == '#') {
      while (**pc != '\n' && **pc != '\0') (*pc)++;
    }
    else break;
  }
}

static int getnum_aux (lua_State *L, lua_State *L1, const char **pc) {
  int res = 0;
  int sig = 1;
  skip(pc);
  if (**pc == '.') {
    res = lua_tointeger(L1, -1);
    lua_pop(L1, 1);
    (*pc)++;
    return res;
  }
  else if (**pc == '-') {
    sig = -1;
    (*pc)++;
  }
  if (!lisdigit(cast_uchar(**pc)))
    luaL_error(L, "number expected (%s)", *pc);
  while (lisdigit(cast_uchar(**pc))) res = res*10 + (*(*pc)++) - '0';
  return sig*res;
}

static const char *getstring_aux (lua_State *L, char *buff, const char **pc) {
  int i = 0;
  skip(pc);
  if (**pc == '"' || **pc == '\'') {  /* quoted string? */
    int quote = *(*pc)++;
    while (**pc != quote) {
      if (**pc == '\0') luaL_error(L, "unfinished string in C script");
      buff[i++] = *(*pc)++;
    }
    (*pc)++;
  }
  else {
    while (**pc != '\0' && !strchr(delimits, **pc))
      buff[i++] = *(*pc)++;
  }
  buff[i] = '\0';
  return buff;
}


static int getindex_aux (lua_State *L, lua_State *L1, const char **pc) {
  skip(pc);
  switch (*(*pc)++) {
    case 'R': return LUA_REGISTRYINDEX;
    case 'G': return luaL_error(L, "deprecated index 'G'");
    case 'U': return lua_upvalueindex(getnum_aux(L, L1, pc));
    default: (*pc)--; return getnum_aux(L, L1, pc);
  }
}


static void pushcode (lua_State *L, int code) {
  static const char *const codes[] = {"OK", "YIELD", "ERRRUN",
                   "ERRSYNTAX", "ERRMEM", "ERRGCMM", "ERRERR"};
  lua_pushstring(L, codes[code]);
}


#define EQ(s1)	(strcmp(s1, inst) == 0)

#define getnum		(getnum_aux(L, L1, &pc))
#define getstring	(getstring_aux(L, buff, &pc))
#define getindex	(getindex_aux(L, L1, &pc))


static int testC (lua_State *L);
static int Cfunck (lua_State *L);

static int runC (lua_State *L, lua_State *L1, const char *pc) {
  char buff[300];
  int status = 0;
  if (pc == NULL) return luaL_error(L, "attempt to runC null script");
  for (;;) {
    const char *inst = getstring;
    if EQ("") return 0;
    else if EQ("absindex") {
      lua_pushnumber(L1, lua_absindex(L1, getindex));
    }
    else if EQ("isnumber") {
      lua_pushboolean(L1, lua_isnumber(L1, getindex));
    }
    else if EQ("isstring") {
      lua_pushboolean(L1, lua_isstring(L1, getindex));
    }
    else if EQ("istable") {
      lua_pushboolean(L1, lua_istable(L1, getindex));
    }
    else if EQ("iscfunction") {
      lua_pushboolean(L1, lua_iscfunction(L1, getindex));
    }
    else if EQ("isfunction") {
      lua_pushboolean(L1, lua_isfunction(L1, getindex));
    }
    else if EQ("isuserdata") {
      lua_pushboolean(L1, lua_isuserdata(L1, getindex));
    }
    else if EQ("isudataval") {
      lua_pushboolean(L1, lua_islightuserdata(L1, getindex));
    }
    else if EQ("isnil") {
      lua_pushboolean(L1, lua_isnil(L1, getindex));
    }
    else if EQ("isnull") {
      lua_pushboolean(L1, lua_isnone(L1, getindex));
    }
    else if EQ("tonumber") {
      lua_pushnumber(L1, lua_tonumber(L1, getindex));
    }
    else if EQ("topointer") {
      lua_pushnumber(L1, cast(size_t, lua_topointer(L1, getindex)));
    }
    else if EQ("tostring") {
      const char *s = lua_tostring(L1, getindex);
      const char *s1 = lua_pushstring(L1, s);
      lua_assert((s == NULL && s1 == NULL) || (strcmp)(s, s1) == 0);
    }
    else if EQ("objsize") {
      lua_pushinteger(L1, lua_rawlen(L1, getindex));
    }
    else if EQ("len") {
      lua_len(L1, getindex);
    }
    else if EQ("Llen") {
      lua_pushinteger(L1, luaL_len(L1, getindex));
    }
    else if EQ("tocfunction") {
      lua_pushcfunction(L1, lua_tocfunction(L1, getindex));
    }
    else if EQ("func2num") {
      lua_CFunction func = lua_tocfunction(L1, getindex);
      lua_pushnumber(L1, cast(size_t, func));
    }
    else if EQ("return") {
      int n = getnum;
      if (L1 != L) {
        int i;
        for (i = 0; i < n; i++)
          lua_pushstring(L, lua_tostring(L1, -(n - i)));
      }
      return n;
    }
    else if EQ("gettop") {
      lua_pushinteger(L1, lua_gettop(L1));
    }
    else if EQ("settop") {
      lua_settop(L1, getnum);
    }
    else if EQ("pop") {
      lua_pop(L1, getnum);
    }
    else if EQ("pushnum") {
      lua_pushinteger(L1, getnum);
    }
    else if EQ("pushstring") {
      lua_pushstring(L1, getstring);
    }
    else if EQ("pushnil") {
      lua_pushnil(L1);
    }
    else if EQ("pushbool") {
      lua_pushboolean(L1, getnum);
    }
    else if EQ("newtable") {
      lua_newtable(L1);
    }
    else if EQ("newuserdata") {
      lua_newuserdata(L1, getnum);
    }
    else if EQ("tobool") {
      lua_pushboolean(L1, lua_toboolean(L1, getindex));
    }
    else if EQ("pushvalue") {
      lua_pushvalue(L1, getindex);
    }
    else if EQ("pushcclosure") {
      lua_pushcclosure(L1, testC, getnum);
    }
    else if EQ("pushupvalueindex") {
      lua_pushinteger(L1, lua_upvalueindex(getnum));
    }
    else if EQ("remove") {
      lua_remove(L1, getnum);
    }
    else if EQ("insert") {
      lua_insert(L1, getnum);
    }
    else if EQ("replace") {
      lua_replace(L1, getindex);
    }
    else if EQ("copy") {
      int f = getindex;
      lua_copy(L1, f, getindex);
    }
    else if EQ("gettable") {
      lua_gettable(L1, getindex);
    }
    else if EQ("getglobal") {
      lua_getglobal(L1, getstring);
    }
    else if EQ("getfield") {
      int t = getindex;
      lua_getfield(L1, t, getstring);
    }
    else if EQ("setfield") {
      int t = getindex;
      lua_setfield(L1, t, getstring);
    }
    else if EQ("rawgeti") {
      int t = getindex;
      lua_rawgeti(L1, t, getnum);
    }
    else if EQ("settable") {
      lua_settable(L1, getindex);
    }
    else if EQ("setglobal") {
      lua_setglobal(L1, getstring);
    }
    else if EQ("next") {
      lua_next(L1, -2);
    }
    else if EQ("concat") {
      lua_concat(L1, getnum);
    }
    else if EQ("print") {
      int n = getnum;
      if (n != 0) {
        printf("%s\n", luaL_tolstring(L1, n, NULL));
        lua_pop(L1, 1);
      }
      else {
        int i;
        n = lua_gettop(L1);
        for (i = 1; i <= n; i++) {
          printf("%s  ", luaL_tolstring(L1, i, NULL));
          lua_pop(L1, 1);
        }
        printf("\n");
      }
    }
    else if EQ("arith") {
      static char ops[] = "+-*/%^_";
      int op;
      skip(&pc);
      op = strchr(ops, *pc++) - ops;
      lua_arith(L1, op);
    }
    else if EQ("compare") {
      int a = getindex;
      int b = getindex;
      lua_pushboolean(L1, lua_compare(L1, a, b, getnum));
    }
    else if EQ("call") {
      int narg = getnum;
      int nres = getnum;
      lua_call(L1, narg, nres);
    }
    else if EQ("pcall") {
      int narg = getnum;
      int nres = getnum;
      status = lua_pcall(L1, narg, nres, 0);
    }
    else if EQ("pcallk") {
      int narg = getnum;
      int nres = getnum;
      int i = getindex;
      status = lua_pcallk(L1, narg, nres, 0, i, Cfunck);
    }
    else if EQ("callk") {
      int narg = getnum;
      int nres = getnum;
      int i = getindex;
      lua_callk(L1, narg, nres, i, Cfunck);
    }
    else if EQ("yield") {
      return lua_yield(L1, getnum);
    }
    else if EQ("yieldk") {
      int nres = getnum;
      int i = getindex;
      return lua_yieldk(L1, nres, i, Cfunck);
    }
    else if EQ("newthread") {
      lua_newthread(L1);
    }
    else if EQ("resume") {
      int i = getindex;
      status = lua_resume(lua_tothread(L1, i), L, getnum);
    }
    else if EQ("pushstatus") {
      pushcode(L1, status);
    }
    else if EQ("xmove") {
      int f = getindex;
      int t = getindex;
      lua_State *fs = (f == 0) ? L1 : lua_tothread(L1, f);
      lua_State *ts = (t == 0) ? L1 : lua_tothread(L1, t);
      int n = getnum;
      if (n == 0) n = lua_gettop(fs);
      lua_xmove(fs, ts, n);
    }
    else if EQ("loadstring") {
      size_t sl;
      const char *s = luaL_checklstring(L1, getnum, &sl);
      luaL_loadbuffer(L1, s, sl, s);
    }
    else if EQ("loadfile") {
      luaL_loadfile(L1, luaL_checkstring(L1, getnum));
    }
    else if EQ("setmetatable") {
      lua_setmetatable(L1, getindex);
    }
    else if EQ("getmetatable") {
      if (lua_getmetatable(L1, getindex) == 0)
        lua_pushnil(L1);
    }
    else if EQ("type") {
      lua_pushstring(L1, luaL_typename(L1, getnum));
    }
    else if EQ("append") {
      int t = getindex;
      int i = lua_rawlen(L1, t);
      lua_rawseti(L1, t, i + 1);
    }
    else if EQ("getctx") {
      int i = 0;
      int s = lua_getctx(L1, &i);
      pushcode(L1, s);
      lua_pushinteger(L1, i);
    }
    else if EQ("checkstack") {
      int sz = getnum;
      luaL_checkstack(L1, sz, getstring);
    }
    else if EQ("newmetatable") {
      lua_pushboolean(L1, luaL_newmetatable(L1, getstring));
    }
    else if EQ("testudata") {
      int i = getindex;
      lua_pushboolean(L1, luaL_testudata(L1, i, getstring) != NULL);
    }
    else if EQ("gsub") {
      int a = getnum; int b = getnum; int c = getnum;
      luaL_gsub(L1, lua_tostring(L1, a),
                    lua_tostring(L1, b),
                    lua_tostring(L1, c));
    }
    else if EQ("sethook") {
      int mask = getnum;
      int count = getnum;
      sethookaux(L1, mask, count, getstring);
    }
    else if EQ("throw") {
#if defined(__cplusplus)
static struct X { int x; } x;
      throw x;
#else
      luaL_error(L1, "C++");
#endif
      break;
    }
    else luaL_error(L, "unknown instruction %s", buff);
  }
  return 0;
}


static int testC (lua_State *L) {
  lua_State *L1;
  const char *pc;
  if (lua_isuserdata(L, 1)) {
    L1 = getstate(L);
    pc = luaL_checkstring(L, 2);
  }
  else if (lua_isthread(L, 1)) {
    L1 = lua_tothread(L, 1);
    pc = luaL_checkstring(L, 2);
  }
  else {
    L1 = L;
    pc = luaL_checkstring(L, 1);
  }
  return runC(L, L1, pc);
}


static int Cfunc (lua_State *L) {
  return runC(L, L, lua_tostring(L, lua_upvalueindex(1)));
}


static int Cfunck (lua_State *L) {
  int i = 0;
  lua_getctx(L, &i);
  return runC(L, L, lua_tostring(L, i));
}


static int makeCfunc (lua_State *L) {
  luaL_checkstring(L, 1);
  lua_pushcclosure(L, Cfunc, lua_gettop(L));
  return 1;
}


/* }====================================================== */


/*
** {======================================================
** tests for C hooks
** =======================================================
*/

/*
** C hook that runs the C script stored in registry.C_HOOK[L]
*/
static void Chook (lua_State *L, lua_Debug *ar) {
  const char *scpt;
  const char *const events [] = {"call", "ret", "line", "count", "tailcall"};
  lua_getfield(L, LUA_REGISTRYINDEX, "C_HOOK");
  lua_pushlightuserdata(L, L);
  lua_gettable(L, -2);  /* get C_HOOK[L] (script saved by sethookaux) */
  scpt = lua_tostring(L, -1);  /* not very religious (string will be popped) */
  lua_pop(L, 2);  /* remove C_HOOK and script */
  lua_pushstring(L, events[ar->event]);  /* may be used by script */
  lua_pushinteger(L, ar->currentline);  /* may be used by script */
  runC(L, L, scpt);  /* run script from C_HOOK[L] */
}


/*
** sets registry.C_HOOK[L] = scpt and sets Chook as a hook
*/
static void sethookaux (lua_State *L, int mask, int count, const char *scpt) {
  if (*scpt == '\0') {  /* no script? */
    lua_sethook(L, NULL, 0, 0);  /* turn off hooks */
    return;
  }
  lua_getfield(L, LUA_REGISTRYINDEX, "C_HOOK");  /* get C_HOOK table */
  if (!lua_istable(L, -1)) {  /* no hook table? */
    lua_pop(L, 1);  /* remove previous value */
    lua_newtable(L);  /* create new C_HOOK table */
    lua_pushvalue(L, -1);
    lua_setfield(L, LUA_REGISTRYINDEX, "C_HOOK");  /* register it */
  }
  lua_pushlightuserdata(L, L);
  lua_pushstring(L, scpt);
  lua_settable(L, -3);  /* C_HOOK[L] = script */
  lua_sethook(L, Chook, mask, count);
}


static int sethook (lua_State *L) {
  if (lua_isnoneornil(L, 1))
    lua_sethook(L, NULL, 0, 0);  /* turn off hooks */
  else {
    const char *scpt = luaL_checkstring(L, 1);
    const char *smask = luaL_checkstring(L, 2);
    int count = luaL_optint(L, 3, 0);
    int mask = 0;
    if (strchr(smask, 'c')) mask |= LUA_MASKCALL;
    if (strchr(smask, 'r')) mask |= LUA_MASKRET;
    if (strchr(smask, 'l')) mask |= LUA_MASKLINE;
    if (count > 0) mask |= LUA_MASKCOUNT;
    sethookaux(L, mask, count, scpt);
  }
  return 0;
}


static int coresume (lua_State *L) {
  int status;
  lua_State *co = lua_tothread(L, 1);
  luaL_argcheck(L, co, 1, "coroutine expected");
  status = lua_resume(co, L, 0);
  if (status != LUA_OK && status != LUA_YIELD) {
    lua_pushboolean(L, 0);
    lua_insert(L, -2);
    return 2;  /* return false + error message */
  }
  else {
    lua_pushboolean(L, 1);
    return 1;
  }
}

/* }====================================================== */



static const struct luaL_Reg tests_funcs[] = {
  {"checkmemory", lua_checkmemory},
  {"closestate", closestate},
  {"d2s", d2s},
  {"doonnewstack", doonnewstack},
  {"doremote", doremote},
  {"gccolor", get_gccolor},
  {"gcstate", gc_state},
  {"getref", getref},
  {"hash", hash_query},
  {"int2fb", int2fb_aux},
  {"limits", get_limits},
  {"listcode", listcode},
  {"listk", listk},
  {"listlocals", listlocals},
  {"loadlib", loadlib},
  {"newstate", newstate},
  {"newuserdata", newuserdata},
  {"num2int", num2int},
  {"pushuserdata", pushuserdata},
  {"querystr", string_query},
  {"querytab", table_query},
  {"ref", tref},
  {"resume", coresume},
  {"s2d", s2d},
  {"sethook", sethook},
  {"stacklevel", stacklevel},
  {"testC", testC},
  {"makeCfunc", makeCfunc},
  {"totalmem", mem_query},
  {"trick", settrick},
  {"udataval", udataval},
  {"unref", unref},
  {"upvalue", upvalue},
  {NULL, NULL}
};


static void checkfinalmem (void) {
  lua_assert(l_memcontrol.numblocks == 0);
  lua_assert(l_memcontrol.total == 0);
}


int luaB_opentests (lua_State *L) {
  void *ud;
  lua_atpanic(L, &tpanic);
  atexit(checkfinalmem);
  lua_assert(lua_getallocf(L, &ud) == debug_realloc);
  lua_assert(ud == cast(void *, &l_memcontrol));
  lua_setallocf(L, lua_getallocf(L, NULL), ud);
  luaL_newlib(L, tests_funcs);
  return 1;
}

#endif
