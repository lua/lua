/*
** $Id: ltests.c,v 1.95 2001/10/26 17:33:30 roberto Exp $
** Internal Module for Debugging of the Lua Implementation
** See Copyright Notice in lua.h
*/


#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define LUA_PRIVATE
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
#include "luadebug.h"
#include "lualib.h"



/*
** The whole module only makes sense with LUA_DEBUG on
*/
#ifdef LUA_DEBUG


lua_State *lua_state = NULL;

int islocked = 0;



static void setnameval (lua_State *L, const l_char *name, int val) {
  lua_pushstring(L, name);
  lua_pushnumber(L, val);
  lua_settable(L, -3);
}


/*
** {======================================================================
** Controlled version for realloc.
** =======================================================================
*/


/* ensures maximum alignment for HEADER */
#define HEADER	(sizeof(union L_Umaxalign))

#define MARKSIZE	32
#define MARK		0x55  /* 01010101 (a nice pattern) */


#define blocksize(b)	(cast(size_t *, b) - HEADER/sizeof(size_t))

unsigned long memdebug_numblocks = 0;
unsigned long memdebug_total = 0;
unsigned long memdebug_maxmem = 0;
unsigned long memdebug_memlimit = ULONG_MAX;


static void *checkblock (void *block) {
  size_t *b = blocksize(block);
  size_t size = *b;
  int i;
  for (i=0;i<MARKSIZE;i++)
    lua_assert(*(cast(char *, b)+HEADER+size+i) == MARK+i); /* corrupted block? */
  return b;
}


static void freeblock (void *block) {
  if (block) {
    size_t size = *blocksize(block);
    block = checkblock(block);
    memset(block, -1, size+HEADER+MARKSIZE);  /* erase block */
    free(block);  /* free original block */
    memdebug_numblocks--;
    memdebug_total -= size;
  }
}


void *debug_realloc (void *block, size_t oldsize, size_t size) {
  lua_assert((oldsize == 0) ? block == NULL : oldsize == *blocksize(block));
  if (size == 0) {
    freeblock(block);
    return NULL;
  }
  else if (memdebug_total+size-oldsize > memdebug_memlimit)
    return NULL;  /* to test memory allocation errors */
  else {
    void *newblock;
    int i;
    size_t realsize = HEADER+size+MARKSIZE;
    if (realsize < size) return NULL;  /* overflow! */
    newblock = malloc(realsize);  /* alloc a new block */
    if (newblock == NULL) return NULL;
    if (oldsize > size) oldsize = size;
    if (block) {
      memcpy(cast(char *, newblock)+HEADER, block, oldsize);
      freeblock(block);  /* erase (and check) old copy */
    }
    /* initialize new part of the block with something `weird' */
    memset(cast(char *, newblock)+HEADER+oldsize, -MARK, size-oldsize);
    memdebug_total += size;
    if (memdebug_total > memdebug_maxmem)
      memdebug_maxmem = memdebug_total;
    memdebug_numblocks++;
    *cast(size_t *, newblock) = size;
    for (i=0;i<MARKSIZE;i++)
      *(cast(char *, newblock)+HEADER+size+i) = cast(char, MARK+i);
    return cast(char *, newblock)+HEADER;
  }
}


/* }====================================================================== */



/*
** {======================================================
** Disassembler
** =======================================================
*/


static l_char *buildop (Proto *p, int pc, l_char *buff) {
  Instruction i = p->code[pc];
  OpCode o = GET_OPCODE(i);
  const l_char *name = luaP_opnames[o];
  sprintf(buff, l_s("%4d - "), pc);
  switch (getOpMode(o)) {  
    case iABC:
      sprintf(buff+strlen(buff), l_s("%-12s%4d %4d %4d"), name,
              GETARG_A(i), GETARG_B(i), GETARG_C(i));
      break;
    case iABc:
      sprintf(buff+strlen(buff), l_s("%-12s%4d %4d"), name, GETARG_A(i), GETARG_Bc(i));
      break;
    case iAsBc:
      sprintf(buff+strlen(buff), l_s("%-12s%4d %4d"), name, GETARG_A(i), GETARG_sBc(i));
      break;
  }
  return buff;
}


static int listcode (lua_State *L) {
  int pc;
  Proto *p;
  luaL_arg_check(L, lua_isfunction(L, 1) && !lua_iscfunction(L, 1),
                 1, l_s("Lua function expected"));
  p = clvalue(luaA_index(L, 1))->l.p;
  lua_newtable(L);
  setnameval(L, l_s("maxstack"), p->maxstacksize);
  setnameval(L, l_s("numparams"), p->numparams);
  for (pc=0; pc<p->sizecode; pc++) {
    l_char buff[100];
    lua_pushnumber(L, pc+1);
    lua_pushstring(L, buildop(p, pc, buff));
    lua_settable(L, -3);
  }
  return 1;
}


static int listk (lua_State *L) {
  Proto *p;
  int i;
  luaL_arg_check(L, lua_isfunction(L, 1) && !lua_iscfunction(L, 1),
                 1, l_s("Lua function expected"));
  p = clvalue(luaA_index(L, 1))->l.p;
  lua_newtable(L);
  for (i=0; i<p->sizek; i++) {
    lua_pushnumber(L, i+1);
    luaA_pushobject(L, p->k+i);
    lua_settable(L, -3);
  }
  return 1;
}


static int listlocals (lua_State *L) {
  Proto *p;
  int pc = luaL_check_int(L, 2) - 1;
  int i = 0;
  const l_char *name;
  luaL_arg_check(L, lua_isfunction(L, 1) && !lua_iscfunction(L, 1),
                 1, l_s("Lua function expected"));
  p = clvalue(luaA_index(L, 1))->l.p;
  while ((name = luaF_getlocalname(p, ++i, pc)) != NULL)
    lua_pushstring(L, name);
  return i-1;
}

/* }====================================================== */




static int get_limits (lua_State *L) {
  lua_newtable(L);
  setnameval(L, l_s("BITS_INT"), BITS_INT);
  setnameval(L, l_s("LFPF"), LFIELDS_PER_FLUSH);
  setnameval(L, l_s("MAXLOCALS"), MAXLOCALS);
  setnameval(L, l_s("MAXPARAMS"), MAXPARAMS);
  setnameval(L, l_s("MAXSTACK"), MAXSTACK);
  setnameval(L, l_s("MAXUPVALUES"), MAXUPVALUES);
  return 1;
}


static int mem_query (lua_State *L) {
  if (lua_isnull(L, 1)) {
    lua_pushnumber(L, memdebug_total);
    lua_pushnumber(L, memdebug_numblocks);
    lua_pushnumber(L, memdebug_maxmem);
    return 3;
  }
  else {
    memdebug_memlimit = luaL_check_int(L, 1);
    return 0;
  }
}


static int hash_query (lua_State *L) {
  if (lua_isnull(L, 2)) {
    luaL_arg_check(L, lua_tag(L, 1) == LUA_TSTRING, 1, l_s("string expected"));
    lua_pushnumber(L, tsvalue(luaA_index(L, 1))->tsv.hash);
  }
  else {
    TObject *o = luaA_index(L, 1);
    Table *t;
    luaL_check_rawtype(L, 2, LUA_TTABLE);
    t = hvalue(luaA_index(L, 2));
    lua_pushnumber(L, luaH_mainposition(t, o) - t->node);
  }
  return 1;
}


static int table_query (lua_State *L) {
  const Table *t;
  int i = luaL_opt_int(L, 2, -1);
  luaL_check_rawtype(L, 1, LUA_TTABLE);
  t = hvalue(luaA_index(L, 1));
  if (i == -1) {
    lua_pushnumber(L, t->sizearray);
    lua_pushnumber(L, sizenode(t));
    lua_pushnumber(L, t->firstfree - t->node);
  }
  else if (i < t->sizearray) {
    lua_pushnumber(L, i);
    luaA_pushobject(L, &t->array[i]);
    lua_pushnil(L); 
  }
  else if ((i -= t->sizearray) < sizenode(t)) {
    if (ttype(val(node(t, i))) != LUA_TNIL ||
        ttype(key(node(t, i))) == LUA_TNIL ||
        ttype(key(node(t, i))) == LUA_TNUMBER) {
      luaA_pushobject(L, key(node(t, i)));
    }
    else
      lua_pushstring(L, "<undef>");
    luaA_pushobject(L, &t->node[i].val);
    if (t->node[i].next)
      lua_pushnumber(L, t->node[i].next - t->node);
    else
      lua_pushnil(L);
  }
  return 3;
}


static int string_query (lua_State *L) {
  stringtable *tb = &G(L)->strt;
  int s = luaL_opt_int(L, 2, 0) - 1;
  if (s==-1) {
    lua_pushnumber(L ,tb->nuse);
    lua_pushnumber(L ,tb->size);
    return 2;
  }
  else if (s < tb->size) {
    TString *ts;
    int n = 0;
    for (ts = tb->hash[s]; ts; ts = ts->tsv.nexthash) {
      setsvalue(L->top, ts);
      incr_top;
      n++;
    }
    return n;
  }
  return 0;
}


static int tref (lua_State *L) {
  int level = lua_gettop(L);
  int lock = luaL_opt_int(L, 2, 1);
  luaL_check_any(L, 1);
  lua_pushvalue(L, 1);
  lua_pushnumber(L, lua_ref(L, lock));
  assert(lua_gettop(L) == level+1);  /* +1 for result */
  return 1;
}

static int getref (lua_State *L) {
  int level = lua_gettop(L);
  lua_getref(L, luaL_check_int(L, 1));
  assert(lua_gettop(L) == level+1);
  return 1;
}

static int unref (lua_State *L) {
  int level = lua_gettop(L);
  lua_unref(L, luaL_check_int(L, 1));
  assert(lua_gettop(L) == level);
  return 0;
}

static int newuserdata (lua_State *L) {
  size_t size = luaL_check_int(L, 1);
  l_char *p = cast(l_char *, lua_newuserdata(L, size));
  while (size--) *p++ = l_c('\0');
  return 1;
}

static int newuserdatabox (lua_State *L) {
  lua_newuserdatabox(L, cast(void *, luaL_check_int(L, 1)));
  return 1;
}

static int settag (lua_State *L) {
  luaL_check_any(L, 1);
  lua_pushvalue(L, 1);  /* push value */
  lua_settag(L, luaL_check_int(L, 2));
  return 1;  /* return value */
}

static int udataval (lua_State *L) {
  luaL_check_rawtype(L, 1, LUA_TUSERDATA);
  lua_pushnumber(L, cast(int, lua_touserdata(L, 1)));
  return 1;
}

static int newtag (lua_State *L) {
  lua_pushnumber(L, lua_newtype(L, lua_tostring(L, 1),
                                   cast(int, lua_tonumber(L, 2))));
  return 1;
}

static int doonnewstack (lua_State *L) {
  lua_State *L1 = lua_newthread(L, luaL_check_int(L, 1));
  if (L1 == NULL) return 0;
  *cast(int **, L1) = &islocked;  /* initialize the lock */
  lua_dostring(L1, luaL_check_string(L, 2));
  lua_pushnumber(L, 1);
  lua_close(L1);
  return 1;
}


static int s2d (lua_State *L) {
  lua_pushnumber(L, *cast(double *, luaL_check_string(L, 1)));
  return 1;
}

static int d2s (lua_State *L) {
  double d = luaL_check_number(L, 1);
  lua_pushlstring(L, cast(l_char *, &d), sizeof(d));
  return 1;
}


static int newstate (lua_State *L) {
  lua_State *L1 = lua_open(luaL_check_int(L, 1));
  if (L1) {
    *cast(int **, L1) = &islocked;  /* initialize the lock */
    lua_pushnumber(L, (unsigned long)L1);
  }
  else
    lua_pushnil(L);
  return 1;
}

static int loadlib (lua_State *L) {
  lua_State *L1 = cast(lua_State *, cast(unsigned long, luaL_check_number(L, 1)));
  lua_register(L1, "mathlibopen", lua_mathlibopen);
  lua_register(L1, "strlibopen", lua_strlibopen);
  lua_register(L1, "iolibopen", lua_iolibopen);
  lua_register(L1, "dblibopen", lua_dblibopen);
  lua_register(L1, "baselibopen", lua_baselibopen);
  return 0;
}

static int closestate (lua_State *L) {
  lua_State *L1 = cast(lua_State *, cast(unsigned long, luaL_check_number(L, 1)));
  lua_close(L1);
  lua_unlock(L);  /* close cannot unlock that */
  return 0;
}

static int doremote (lua_State *L) {
  lua_State *L1;
  const l_char *code = luaL_check_string(L, 2);
  int status;
  L1 = cast(lua_State *, cast(unsigned long, luaL_check_number(L, 1)));
  status = lua_dostring(L1, code);
  if (status != 0) {
    lua_pushnil(L);
    lua_pushnumber(L, status);
    return 2;
  }
  else {
    int i = 0;
    while (!lua_isnull(L1, ++i))
      lua_pushstring(L, lua_tostring(L1, i));
    lua_pop(L1, i-1);
    return i-1;
  }
}

static int settagmethod (lua_State *L) {
  int tag = luaL_check_int(L, 1);
  const l_char *event = luaL_check_string(L, 2);
  luaL_check_any(L, 3);
  lua_gettagmethod(L, tag, event);
  lua_pushvalue(L, 3);
  lua_settagmethod(L, tag, event);
  return 1;
}


static int log2_aux (lua_State *L) {
  lua_pushnumber(L, luaO_log2(luaL_check_int(L, 1)));
  return 1;
}


/*
** {======================================================
** function to test the API with C. It interprets a kind of assembler
** language with calls to the API, so the test can be driven by Lua code
** =======================================================
*/

static const l_char *const delimits = l_s(" \t\n,;");

static void skip (const l_char **pc) {
  while (**pc != l_c('\0') && strchr(delimits, **pc)) (*pc)++;
}

static int getnum_aux (lua_State *L, const l_char **pc) {
  int res = 0;
  int sig = 1;
  skip(pc);
  if (**pc == l_c('.')) {
    res = cast(int, lua_tonumber(L, -1));
    lua_pop(L, 1);
    (*pc)++;
    return res;
  }
  else if (**pc == l_c('-')) {
    sig = -1;
    (*pc)++;
  }
  while (isdigit(cast(int, **pc))) res = res*10 + (*(*pc)++) - l_c('0');
  return sig*res;
}
  
static const l_char *getname_aux (l_char *buff, const l_char **pc) {
  int i = 0;
  skip(pc);
  while (**pc != l_c('\0') && !strchr(delimits, **pc))
    buff[i++] = *(*pc)++;
  buff[i] = l_c('\0');
  return buff;
}


#define EQ(s1)	(strcmp(s1, inst) == 0)

#define getnum	(getnum_aux(L, &pc))
#define getname	(getname_aux(buff, &pc))


static int testC (lua_State *L) {
  l_char buff[30];
  const l_char *pc = luaL_check_string(L, 1);
  for (;;) {
    const l_char *inst = getname;
    if EQ(l_s("")) return 0;
    else if EQ(l_s("isnumber")) {
      lua_pushnumber(L, lua_isnumber(L, getnum));
    }
    else if EQ(l_s("isstring")) {
      lua_pushnumber(L, lua_isstring(L, getnum));
    }
    else if EQ(l_s("istable")) {
      lua_pushnumber(L, lua_istable(L, getnum));
    }
    else if EQ(l_s("iscfunction")) {
      lua_pushnumber(L, lua_iscfunction(L, getnum));
    }
    else if EQ(l_s("isfunction")) {
      lua_pushnumber(L, lua_isfunction(L, getnum));
    }
    else if EQ(l_s("isuserdata")) {
      lua_pushnumber(L, lua_isuserdata(L, getnum));
    }
    else if EQ(l_s("isnil")) {
      lua_pushnumber(L, lua_isnil(L, getnum));
    }
    else if EQ(l_s("isnull")) {
      lua_pushnumber(L, lua_isnull(L, getnum));
    }
    else if EQ(l_s("tonumber")) {
      lua_pushnumber(L, lua_tonumber(L, getnum));
    }
    else if EQ(l_s("tostring")) {
      const l_char *s = lua_tostring(L, getnum);
      lua_pushstring(L, s);
    }
    else if EQ(l_s("tonumber")) {
      lua_pushnumber(L, lua_tonumber(L, getnum));
    }
    else if EQ(l_s("strlen")) {
      lua_pushnumber(L, lua_strlen(L, getnum));
    }
    else if EQ(l_s("tocfunction")) {
      lua_pushcfunction(L, lua_tocfunction(L, getnum));
    }
    else if EQ(l_s("return")) {
      return getnum;
    }
    else if EQ(l_s("gettop")) {
      lua_pushnumber(L, lua_gettop(L));
    }
    else if EQ(l_s("settop")) {
      lua_settop(L, getnum);
    }
    else if EQ(l_s("pop")) {
      lua_pop(L, getnum);
    }
    else if EQ(l_s("pushnum")) {
      lua_pushnumber(L, getnum);
    }
    else if EQ(l_s("pushvalue")) {
      lua_pushvalue(L, getnum);
    }
    else if EQ(l_s("pushcclosure")) {
      lua_pushcclosure(L, testC, getnum);
    }
    else if EQ(l_s("pushupvalues")) {
      lua_pushupvalues(L);
    }
    else if EQ(l_s("remove")) {
      lua_remove(L, getnum);
    }
    else if EQ(l_s("insert")) {
      lua_insert(L, getnum);
    }
    else if EQ(l_s("gettable")) {
      lua_gettable(L, getnum);
    }
    else if EQ(l_s("settable")) {
      lua_settable(L, getnum);
    }
    else if EQ(l_s("next")) {
      lua_next(L, -2);
    }
    else if EQ(l_s("concat")) {
      lua_concat(L, getnum);
    }
    else if EQ(l_s("lessthan")) {
      int a = getnum;
      if (lua_lessthan(L, a, getnum))
        lua_pushnumber(L, 1);
      else
        lua_pushnil(L);
    }
    else if EQ(l_s("equal")) {
      int a = getnum;
      if (lua_equal(L, a, getnum))
        lua_pushnumber(L, 1);
      else
        lua_pushnil(L);
    }
    else if EQ(l_s("rawcall")) {
      int narg = getnum;
      int nres = getnum;
      lua_rawcall(L, narg, nres);
    }
    else if EQ(l_s("call")) {
      int narg = getnum;
      int nres = getnum;
      lua_call(L, narg, nres);
    }
    else if EQ(l_s("dostring")) {
      lua_dostring(L, luaL_check_string(L, getnum));
    }
    else if EQ(l_s("settagmethod")) {
      int tag = getnum;
      const l_char *event = getname;
      lua_settagmethod(L, tag, event);
    }
    else if EQ(l_s("gettagmethod")) {
      int tag = getnum;
      const l_char *event = getname;
      lua_gettagmethod(L, tag, event);
    }
    else if EQ(l_s("type")) {
      lua_pushstring(L, lua_type(L, getnum));
    }
    else luaL_verror(L, l_s("unknown instruction %.30s"), buff);
  }
  return 0;
}

/* }====================================================== */



static const struct luaL_reg tests_funcs[] = {
  {l_s("hash"), hash_query},
  {l_s("limits"), get_limits},
  {l_s("listcode"), listcode},
  {l_s("listk"), listk},
  {l_s("listlocals"), listlocals},
  {l_s("loadlib"), loadlib},
  {l_s("querystr"), string_query},
  {l_s("querytab"), table_query},
  {l_s("testC"), testC},
  {l_s("ref"), tref},
  {l_s("getref"), getref},
  {l_s("unref"), unref},
  {l_s("d2s"), d2s},
  {l_s("s2d"), s2d},
  {l_s("newuserdata"), newuserdata},
  {l_s("newuserdatabox"), newuserdatabox},
  {l_s("settag"), settag},
  {l_s("udataval"), udataval},
  {l_s("newtag"), newtag},
  {l_s("doonnewstack"), doonnewstack},
  {l_s("newstate"), newstate},
  {l_s("closestate"), closestate},
  {l_s("doremote"), doremote},
  {l_s("settagmethod"), settagmethod},
  {l_s("log2"), log2_aux},
  {l_s("totalmem"), mem_query}
};


void luaB_opentests (lua_State *L) {
  *cast(int **, L) = &islocked;  /* init lock */
  lua_state = L;  /* keep first state to be opened */
  /* open lib in a new table */
  lua_newtable(L);
  lua_getglobals(L);
  lua_pushvalue(L, -2);
  lua_setglobals(L);
  luaL_openl(L, tests_funcs);  /* open functions inside new table */
  lua_setglobals(L);  /* restore old table of globals */
  lua_setglobal(L, l_s("T"));  /* set new table as global T */
}

#endif
