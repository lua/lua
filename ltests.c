/*
** $Id: ltests.c,v 2.35 2006/01/10 12:50:00 roberto Exp roberto $
** Internal Module for Debugging of the Lua Implementation
** See Copyright Notice in lua.h
*/


#include <ctype.h>
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


int Trick = 0;


static lua_State *lua_state = NULL;

int islocked = 0;


#define obj_at(L,k)	(L->ci->base+(k) - 1)


static void setnameval (lua_State *L, const char *name, int val) {
  lua_pushstring(L, name);
  lua_pushinteger(L, val);
  lua_settable(L, -3);
}


/*
** {======================================================================
** Controlled version for realloc.
** =======================================================================
*/

#define MARK		0x55  /* 01010101 (a nice pattern) */

#ifndef EXTERNMEMCHECK
/* full memory check */
#define HEADER	(sizeof(L_Umaxalign)) /* ensures maximum alignment for HEADER */
#define MARKSIZE	16  /* size of marks after each block */
#define blockhead(b)	(cast(char *, b) - HEADER)
#define setsize(newblock, size)	(*cast(size_t *, newblock) = size)
#define checkblocksize(b, size) (size == (*cast(size_t *, blockhead(b))))
#define fillmem(mem,size)	memset(mem, -MARK, size)
#else
/* external memory check: don't do it twice */
#define HEADER		0
#define MARKSIZE	0
#define blockhead(b)	(b)
#define setsize(newblock, size)	/* empty */
#define checkblocksize(b,size)	(1)
#define fillmem(mem,size)	/* empty */
#endif


Memcontrol memcontrol = {0L, 0L, 0L, ULONG_MAX};


static void *checkblock (void *block, size_t size) {
  void *b = blockhead(block);
  int i;
  for (i=0;i<MARKSIZE;i++)
    lua_assert(*(cast(char *, b)+HEADER+size+i) == MARK+i); /* corrupted block? */
  return b;
}


static void freeblock (Memcontrol *mc, void *block, size_t size) {
  if (block) {
    lua_assert(checkblocksize(block, size));
    block = checkblock(block, size);
    fillmem(block, size+HEADER+MARKSIZE);  /* erase block */
    free(block);  /* free original block */
    mc->numblocks--;
    mc->total -= size;
  }
}


void *debug_realloc (void *ud, void *block, size_t oldsize, size_t size) {
  Memcontrol *mc = cast(Memcontrol *, ud);
  lua_assert(oldsize == 0 || checkblocksize(block, oldsize));
  if (size == 0) {
    freeblock(mc, block, oldsize);
    return NULL;
  }
  else if (size > oldsize && mc->total+size-oldsize > mc->memlimit)
    return NULL;  /* to test memory allocation errors */
  else {
    void *newblock;
    int i;
    size_t realsize = HEADER+size+MARKSIZE;
    size_t commonsize = (oldsize < size) ? oldsize : size;
    if (realsize < size) return NULL;  /* overflow! */
    newblock = malloc(realsize);  /* alloc a new block */
    if (newblock == NULL) return NULL;
    if (block) {
      memcpy(cast(char *, newblock)+HEADER, block, commonsize);
      freeblock(mc, block, oldsize);  /* erase (and check) old copy */
    }
    /* initialize new part of the block with something `weird' */
    fillmem(cast(char *, newblock)+HEADER+commonsize, size-commonsize);
    mc->total += size;
    if (mc->total > mc->maxmem)
      mc->maxmem = mc->total;
    mc->numblocks++;
    setsize(newblock, size);
    for (i=0;i<MARKSIZE;i++)
      *(cast(char *, newblock)+HEADER+size+i) = cast(char, MARK+i);
    return cast(char *, newblock)+HEADER;
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
  if (g->gcstate == GCSpropagate)
    return !isblack(f) || !iswhite(t);
  else if (g->gcstate == GCSfinalize)
    return iswhite(f);
  else
    return 1;
}


static void printobj (global_State *g, GCObject *o) {
  int i = 0;
  GCObject *p;
  for (p = g->rootgc; p != o && p != NULL; p = p->gch.next) i++;
  if (p == NULL) i = -1;
  printf("%d:%s(%p)-%c(%02X)", i, luaT_typenames[o->gch.tt], (void *)o,
           isdead(g,o)?'d':isblack(o)?'b':iswhite(o)?'w':'g', o->gch.marked);
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

#define checkvalref(g,f,t) lua_assert(!iscollectable(t) || \
	((ttype(t) == (t)->value.gc->gch.tt) && testobjref(g,f,gcvalue(t))))



static void checktable (global_State *g, Table *h) {
  int i;
  int weakkey = 0;
  int weakvalue = 0;
  const TValue *mode;
  GCObject *hgc = obj2gco(h);
  if (h->metatable)
    checkobjref(g, hgc, h->metatable);
  mode = gfasttm(g, h->metatable, TM_MODE);
  if (mode && ttisstring(mode)) {  /* is there a weak mode? */
    weakkey = (strchr(svalue(mode), 'k') != NULL);
    weakvalue = (strchr(svalue(mode), 'v') != NULL);
  }
  i = h->sizearray;
  while (i--)
    checkvalref(g, hgc, &h->array[i]);
  i = sizenode(h);
  while (i--) {
    Node *n = gnode(h, i);
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
    if (f->upvalues[i])
      checkobjref(g, fgc, f->upvalues[i]);
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



static void checkclosure (global_State *g, Closure *cl) {
  GCObject *clgc = obj2gco(cl);
  checkobjref(g, clgc, cl->l.env);
  if (cl->c.isC) {
    int i;
    for (i=0; i<cl->c.nupvalues; i++)
      checkvalref(g, clgc, &cl->c.upvalue[i]);
  }
  else {
    int i;
    lua_assert(cl->l.nupvalues == cl->l.p->nups);
    checkobjref(g, clgc, cl->l.p);
    for (i=0; i<cl->l.nupvalues; i++) {
      if (cl->l.upvals[i]) {
        lua_assert(cl->l.upvals[i]->tt == LUA_TUPVAL);
        checkobjref(g, clgc, cl->l.upvals[i]);
      }
    }
  }
}


static void checkstack (global_State *g, lua_State *L1) {
  StkId o;
  CallInfo *ci;
  GCObject *uvo;
  lua_assert(!isdead(g, obj2gco(L1)));
  for (uvo = L1->openupval; uvo != NULL; uvo = uvo->gch.next) {
    UpVal *uv = gco2uv(uvo);
    lua_assert(uv->v != &uv->u.value);  /* must be open */
    lua_assert(!isblack(uvo));  /* open upvalues cannot be black */
  }
  checkliveness(g, gt(L1));
  if (L1->base_ci) {
    for (ci = L1->base_ci; ci <= L1->ci; ci++) {
      lua_assert(ci->top <= L1->stack_last);
      lua_assert(lua_checkpc(L1, ci));
    }
  }
  else lua_assert(L1->size_ci == 0);
  if (L1->stack) {
    for (o = L1->stack; o < L1->top; o++)
      checkliveness(g, o);
  }
  else lua_assert(L1->stacksize == 0);
}


static void checkobject (global_State *g, GCObject *o) {
  if (isdead(g, o))
/*    lua_assert(g->gcstate == GCSsweepstring || g->gcstate == GCSsweep);*/
{ if (!(g->gcstate == GCSsweepstring || g->gcstate == GCSsweep))
printf(">>> %d  %s  %02x\n", g->gcstate, luaT_typenames[o->gch.tt], o->gch.marked);
}
  else {
    if (g->gcstate == GCSfinalize)
      lua_assert(iswhite(o));
    switch (o->gch.tt) {
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
        checktable(g, gco2h(o));
        break;
      }
      case LUA_TTHREAD: {
        checkstack(g, gco2th(o));
        break;
      }
      case LUA_TFUNCTION: {
        checkclosure(g, gco2cl(o));
        break;
      }
      case LUA_TPROTO: {
        checkproto(g, gco2p(o));
        break;
      }
      default: lua_assert(0);
    }
  }
}


int lua_checkpc (lua_State *L, pCallInfo ci) {
  if (ci == L->base_ci || !f_isLua(ci)) return 1;
  else {
    Proto *p = ci_func(ci)->l.p;
    if (ci < L->ci)
      return p->code <= ci->savedpc && ci->savedpc <= p->code + p->sizecode;
    else
      return p->code <= L->savedpc && L->savedpc <= p->code + p->sizecode;
  }
}


int lua_checkmemory (lua_State *L) {
  global_State *g = G(L);
  GCObject *o;
  UpVal *uv;
  checkstack(g, g->mainthread);
  for (o = g->rootgc; o != obj2gco(g->mainthread); o = o->gch.next)
    checkobject(g, o);
  for (o = o->gch.next; o != NULL; o = o->gch.next) {
    lua_assert(o->gch.tt == LUA_TUSERDATA);
    checkobject(g, o);
  }
  for (uv = g->uvhead.u.l.next; uv != &g->uvhead; uv = uv->u.l.next) {
    lua_assert(uv->u.l.next->u.l.prev == uv && uv->u.l.prev->u.l.next == uv);
    lua_assert(uv->v != &uv->u.value);  /* must be open */
    lua_assert(!isblack(obj2gco(uv)));  /* open upvalues are never black */
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
  int line = getline(p, pc);
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
#endif


static int listcode (lua_State *L) {
  int pc;
  Proto *p;
  luaL_argcheck(L, lua_isfunction(L, 1) && !lua_iscfunction(L, 1),
                 1, "Lua function expected");
  p = clvalue(obj_at(L, 1))->l.p;
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
  p = clvalue(obj_at(L, 1))->l.p;
  lua_createtable(L, p->sizek, 0);
  for (i=0; i<p->sizek; i++) {
    luaA_pushobject(L, p->k+i);
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
  p = clvalue(obj_at(L, 1))->l.p;
  while ((name = luaF_getlocalname(p, ++i, pc)) != NULL)
    lua_pushstring(L, name);
  return i-1;
}

/* }====================================================== */




static int get_limits (lua_State *L) {
  lua_createtable(L, 0, 5);
  setnameval(L, "BITS_INT", LUAI_BITSINT);
  setnameval(L, "LFPF", LFIELDS_PER_FLUSH);
  setnameval(L, "MAXVARS", LUAI_MAXVARS);
  setnameval(L, "MAXSTACK", MAXSTACK);
  setnameval(L, "MAXUPVALUES", LUAI_MAXUPVALUES);
  setnameval(L, "NUM_OPCODES", NUM_OPCODES);
  return 1;
}


static int mem_query (lua_State *L) {
  if (lua_isnone(L, 1)) {
    lua_pushinteger(L, memcontrol.total);
    lua_pushinteger(L, memcontrol.numblocks);
    lua_pushinteger(L, memcontrol.maxmem);
    return 3;
  }
  else {
    memcontrol.memlimit = luaL_checkint(L, 1);
    return 0;
  }
}


static int settrick (lua_State *L) {
  Trick = lua_tointeger(L, 1);
  return 0;
}


/*static int set_gcstate (lua_State *L) {
  static const char *const state[] = {"propagate", "sweep", "finalize"};
  return 0;
}*/


static int get_gccolor (lua_State *L) {
  TValue *o;
  luaL_checkany(L, 1);
  o = obj_at(L, 1);
  if (!iscollectable(o))
    lua_pushstring(L, "no collectable");
  else
    lua_pushstring(L, iswhite(gcvalue(o)) ? "white" :
                      isblack(gcvalue(o)) ? "black" : "grey");
  return 1;
}


static int gcstate (lua_State *L) {
  switch(G(L)->gcstate) {
    case GCSpropagate: lua_pushstring(L, "propagate"); break;
    case GCSsweepstring: lua_pushstring(L, "sweep strings"); break;
    case GCSsweep: lua_pushstring(L, "sweep"); break;
    case GCSfinalize: lua_pushstring(L, "finalize"); break;
  }
  return 1;
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
  lua_pushinteger(L, (L->ci - L->base_ci));
  lua_pushinteger(L, (L->end_ci - L->base_ci));
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
    luaA_pushobject(L, &t->array[i]);
    lua_pushnil(L); 
  }
  else if ((i -= t->sizearray) < sizenode(t)) {
    if (!ttisnil(gval(gnode(t, i))) ||
        ttisnil(gkey(gnode(t, i))) ||
        ttisnumber(gkey(gnode(t, i)))) {
      luaA_pushobject(L, key2tval(gnode(t, i)));
    }
    else
      lua_pushliteral(L, "<undef>");
    luaA_pushobject(L, gval(gnode(t, i)));
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
    for (ts = tb->hash[s]; ts; ts = ts->gch.next) {
      setsvalue2s(L, L->top, gco2ts(ts));
      incr_top(L);
      n++;
    }
    return n;
  }
  return 0;
}


static int tref (lua_State *L) {
  int level = lua_gettop(L);
  int lock = luaL_optint(L, 2, 1);
  luaL_checkany(L, 1);
  lua_pushvalue(L, 1);
  lua_pushinteger(L, lua_ref(L, lock));
  lua_assert(lua_gettop(L) == level+1);  /* +1 for result */
  return 1;
}

static int getref (lua_State *L) {
  int level = lua_gettop(L);
  lua_getref(L, luaL_checkint(L, 1));
  lua_assert(lua_gettop(L) == level+1);
  return 1;
}

static int unref (lua_State *L) {
  int level = lua_gettop(L);
  lua_unref(L, luaL_checkint(L, 1));
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
  lua_pushlightuserdata(L, cast(void *, luaL_checkint(L, 1)));
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
  if (status == 0)
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
  if (L1)
    lua_pushinteger(L, (unsigned long)L1);
  else
    lua_pushnil(L);
  return 1;
}


static int loadlib (lua_State *L) {
  static const luaL_Reg libs[] = {
    {"baselibopen", luaopen_base},
    {"dblibopen", luaopen_debug},
    {"iolibopen", luaopen_io},
    {"mathlibopen", luaopen_math},
    {"strlibopen", luaopen_string},
    {"tablibopen", luaopen_table},
    {"packageopen", luaopen_package},
    {NULL, NULL}
  };
  lua_State *L1 = cast(lua_State *,
                       cast(unsigned long, luaL_checknumber(L, 1)));
  lua_pushvalue(L1, LUA_GLOBALSINDEX);
  luaL_register(L1, NULL, libs);
  return 0;
}

static int closestate (lua_State *L) {
  lua_State *L1 = cast(lua_State *, cast(unsigned long, luaL_checknumber(L, 1)));
  lua_close(L1);
  return 0;
}

static int doremote (lua_State *L) {
  lua_State *L1 = cast(lua_State *,cast(unsigned long,luaL_checknumber(L, 1)));
  size_t lcode;
  const char *code = luaL_checklstring(L, 2, &lcode);
  int status;
  lua_settop(L1, 0);
  status = luaL_loadbuffer(L1, code, lcode, code);
  if (status == 0)
    status = lua_pcall(L1, 0, LUA_MULTRET, 0);
  if (status != 0) {
    lua_pushnil(L);
    lua_pushinteger(L, status);
    lua_pushstring(L, lua_tostring(L1, -1));
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


static int log2_aux (lua_State *L) {
  lua_pushinteger(L, luaO_log2(luaL_checkint(L, 1)));
  return 1;
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

static const char *const delimits = " \t\n,;";

static void skip (const char **pc) {
  while (**pc != '\0' && strchr(delimits, **pc)) (*pc)++;
}

static int getnum_aux (lua_State *L, const char **pc) {
  int res = 0;
  int sig = 1;
  skip(pc);
  if (**pc == '.') {
    res = cast_int(lua_tonumber(L, -1));
    lua_pop(L, 1);
    (*pc)++;
    return res;
  }
  else if (**pc == '-') {
    sig = -1;
    (*pc)++;
  }
  while (isdigit(cast_int(**pc))) res = res*10 + (*(*pc)++) - '0';
  return sig*res;
}
  
static const char *getname_aux (char *buff, const char **pc) {
  int i = 0;
  skip(pc);
  while (**pc != '\0' && !strchr(delimits, **pc))
    buff[i++] = *(*pc)++;
  buff[i] = '\0';
  return buff;
}


static int getindex_aux (lua_State *L, const char **pc) {
  skip(pc);
  switch (*(*pc)++) {
    case 'R': return LUA_REGISTRYINDEX;
    case 'G': return LUA_GLOBALSINDEX;
    case 'E': return LUA_ENVIRONINDEX;
    case 'U': return lua_upvalueindex(getnum_aux(L, pc));
    default: (*pc)--; return getnum_aux(L, pc);
  }
}

#define EQ(s1)	(strcmp(s1, inst) == 0)

#define getnum	(getnum_aux(L, &pc))
#define getname	(getname_aux(buff, &pc))
#define getindex (getindex_aux(L, &pc))


static int testC (lua_State *L) {
  char buff[30];
  lua_State *L1;
  const char *pc;
  if (lua_isnumber(L, 1)) {
    L1 = cast(lua_State *,cast(unsigned long,luaL_checknumber(L, 1)));
    pc = luaL_checkstring(L, 2);
  }
  else {
    L1 = L;
    pc = luaL_checkstring(L, 1);
  }
  for (;;) {
    const char *inst = getname;
    if EQ("") return 0;
    else if EQ("isnumber") {
      lua_pushinteger(L1, lua_isnumber(L1, getindex));
    }
    else if EQ("isstring") {
      lua_pushinteger(L1, lua_isstring(L1, getindex));
    }
    else if EQ("istable") {
      lua_pushinteger(L1, lua_istable(L1, getindex));
    }
    else if EQ("iscfunction") {
      lua_pushinteger(L1, lua_iscfunction(L1, getindex));
    }
    else if EQ("isfunction") {
      lua_pushinteger(L1, lua_isfunction(L1, getindex));
    }
    else if EQ("isuserdata") {
      lua_pushinteger(L1, lua_isuserdata(L1, getindex));
    }
    else if EQ("isudataval") {
      lua_pushinteger(L1, lua_islightuserdata(L1, getindex));
    }
    else if EQ("isnil") {
      lua_pushinteger(L1, lua_isnil(L1, getindex));
    }
    else if EQ("isnull") {
      lua_pushinteger(L1, lua_isnone(L1, getindex));
    }
    else if EQ("tonumber") {
      lua_pushnumber(L1, lua_tonumber(L1, getindex));
    }
    else if EQ("tostring") {
      const char *s = lua_tostring(L1, getindex);
      lua_pushstring(L1, s);
    }
    else if EQ("objsize") {
      lua_pushinteger(L1, lua_objlen(L1, getindex));
    }
    else if EQ("tocfunction") {
      lua_pushcfunction(L1, lua_tocfunction(L1, getindex));
    }
    else if EQ("return") {
      return getnum;
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
      lua_pushstring(L1, getname);
    }
    else if EQ("pushnil") {
      lua_pushnil(L1);
    }
    else if EQ("pushbool") {
      lua_pushboolean(L1, getnum);
    }
    else if EQ("newuserdata") {
      lua_newuserdata(L1, getnum);
    }
    else if EQ("tobool") {
      lua_pushinteger(L1, lua_toboolean(L1, getindex));
    }
    else if EQ("pushvalue") {
      lua_pushvalue(L1, getindex);
    }
    else if EQ("pushcclosure") {
      lua_pushcclosure(L1, testC, getnum);
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
    else if EQ("gettable") {
      lua_gettable(L1, getindex);
    }
    else if EQ("settable") {
      lua_settable(L1, getindex);
    }
    else if EQ("next") {
      lua_next(L1, -2);
    }
    else if EQ("concat") {
      lua_concat(L1, getnum);
    }
    else if EQ("lessthan") {
      int a = getindex;
      lua_pushboolean(L1, lua_lessthan(L1, a, getindex));
    }
    else if EQ("equal") {
      int a = getindex;
      lua_pushboolean(L1, lua_equal(L1, a, getindex));
    }
    else if EQ("rawcall") {
      int narg = getnum;
      int nres = getnum;
      lua_call(L1, narg, nres);
    }
    else if EQ("call") {
      int narg = getnum;
      int nres = getnum;
      lua_pcall(L1, narg, nres, 0);
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
    else if EQ("getn") {
      int i = getindex;
      lua_pushinteger(L1, luaL_getn(L1, i));
    }
#ifndef luaL_setn
    else if EQ("setn") {
      int i = getindex;
      int n = cast_int(lua_tonumber(L1, -1));
      luaL_setn(L1, i, n);
      lua_pop(L1, 1);
    }
#endif
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

/* }====================================================== */


/*
** {======================================================
** tests for yield inside hooks
** =======================================================
*/

static void yieldf (lua_State *L, lua_Debug *ar) {
  lua_yield(L, 0);
}

static int setyhook (lua_State *L) {
  if (lua_isnoneornil(L, 1))
    lua_sethook(L, NULL, 0, 0);  /* turn off hooks */
  else {
    const char *smask = luaL_checkstring(L, 1);
    int count = luaL_optint(L, 2, 0);
    int mask = 0;
    if (strchr(smask, 'l')) mask |= LUA_MASKLINE;
    if (count > 0) mask |= LUA_MASKCOUNT;
    lua_sethook(L, yieldf, mask, count);
  }
  return 0;
}


static int coresume (lua_State *L) {
  int status;
  lua_State *co = lua_tothread(L, 1);
  luaL_argcheck(L, co, 1, "coroutine expected");
  status = lua_resume(co, 0);
  if (status != 0) {
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



/*
** {======================================================
** tests auxlib functions
** =======================================================
*/

static int auxgsub (lua_State *L) {
  const char *s1 = luaL_checkstring(L, 1);
  const char *s2 = luaL_checkstring(L, 2);
  const char *s3 = luaL_checkstring(L, 3);
  lua_settop(L, 3);
  luaL_gsub(L, s1, s2, s3);
  lua_assert(lua_gettop(L) == 4);
  return 1;
}


/* }====================================================== */



static const struct luaL_Reg tests_funcs[] = {
  {"checkmemory", lua_checkmemory},
  {"closestate", closestate},
  {"d2s", d2s},
  {"doonnewstack", doonnewstack},
  {"doremote", doremote},
  {"gccolor", get_gccolor},
  {"gcstate", gcstate},
  {"getref", getref},
  {"gsub", auxgsub},
  {"hash", hash_query},
  {"int2fb", int2fb_aux},
  {"limits", get_limits},
  {"listcode", listcode},
  {"listk", listk},
  {"listlocals", listlocals},
  {"loadlib", loadlib},
  {"log2", log2_aux},
  {"newstate", newstate},
  {"newuserdata", newuserdata},
  {"num2int", num2int},
  {"pushuserdata", pushuserdata},
  {"querystr", string_query},
  {"querytab", table_query},
  {"ref", tref},
  {"resume", coresume},
  {"s2d", s2d},
  {"setyhook", setyhook},
  {"stacklevel", stacklevel},
  {"testC", testC},
  {"totalmem", mem_query},
  {"trick", settrick},
  {"udataval", udataval},
  {"unref", unref},
  {"upvalue", upvalue},
  {NULL, NULL}
};


int luaB_opentests (lua_State *L) {
  void *ud;
  lua_assert(lua_getallocf(L, &ud) == debug_realloc);
  lua_assert(ud == cast(void *, &memcontrol));
  lua_setallocf(L, lua_getallocf(L, NULL), ud);
  lua_state = L;  /* keep first state to be opened */
  luaL_register(L, "T", tests_funcs);
  return 0;
}


#undef main
int main (int argc, char *argv[]) {
  int ret;
  char *limit = getenv("MEMLIMIT");
  if (limit)
    memcontrol.memlimit = strtoul(limit, NULL, 10);
  ret = l_main(argc, argv);
  lua_assert(memcontrol.numblocks == 0);
  lua_assert(memcontrol.total == 0);
  return ret;
}

#endif
