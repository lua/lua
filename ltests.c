/*
** $Id: ltests.c,v 1.116 2002/04/05 18:54:31 roberto Exp roberto $
** Internal Module for Debugging of the Lua Implementation
** See Copyright Notice in lua.h
*/


#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


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


static lua_State *lua_state = NULL;

int islocked = 0;


#define index(L,k)	(L->ci->base+(k) - 1)


static void setnameval (lua_State *L, const char *name, int val) {
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


static char *buildop (Proto *p, int pc, char *buff) {
  Instruction i = p->code[pc];
  OpCode o = GET_OPCODE(i);
  const char *name = luaP_opnames[o];
  int line = p->lineinfo[pc];
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


static int listcode (lua_State *L) {
  int pc;
  Proto *p;
  luaL_arg_check(L, lua_isfunction(L, 1) && !lua_iscfunction(L, 1),
                 1, "Lua function expected");
  p = clvalue(index(L, 1))->l.p;
  lua_newtable(L);
  setnameval(L, "maxstack", p->maxstacksize);
  setnameval(L, "numparams", p->numparams);
  for (pc=0; pc<p->sizecode; pc++) {
    char buff[100];
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
                 1, "Lua function expected");
  p = clvalue(index(L, 1))->l.p;
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
  const char *name;
  luaL_arg_check(L, lua_isfunction(L, 1) && !lua_iscfunction(L, 1),
                 1, "Lua function expected");
  p = clvalue(index(L, 1))->l.p;
  while ((name = luaF_getlocalname(p, ++i, pc)) != NULL)
    lua_pushstring(L, name);
  return i-1;
}

/* }====================================================== */




static int get_limits (lua_State *L) {
  lua_newtable(L);
  setnameval(L, "BITS_INT", BITS_INT);
  setnameval(L, "LFPF", LFIELDS_PER_FLUSH);
  setnameval(L, "MAXVARS", MAXVARS);
  setnameval(L, "MAXPARAMS", MAXPARAMS);
  setnameval(L, "MAXSTACK", MAXSTACK);
  setnameval(L, "MAXUPVALUES", MAXUPVALUES);
  return 1;
}


static int mem_query (lua_State *L) {
  if (lua_isnone(L, 1)) {
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
  if (lua_isnone(L, 2)) {
    luaL_arg_check(L, lua_type(L, 1) == LUA_TSTRING, 1, "string expected");
    lua_pushnumber(L, tsvalue(index(L, 1))->tsv.hash);
  }
  else {
    TObject *o = index(L, 1);
    Table *t;
    luaL_check_type(L, 2, LUA_TTABLE);
    t = hvalue(index(L, 2));
    lua_pushnumber(L, luaH_mainposition(t, o) - t->node);
  }
  return 1;
}


static int stacklevel (lua_State *L) {
  unsigned long a = 0;
  lua_pushnumber(L, (int)(L->top - L->stack));
  lua_pushnumber(L, (int)(L->stack_last - L->stack));
  lua_pushnumber(L, (int)(L->ci - L->base_ci));
  lua_pushnumber(L, (int)(L->end_ci - L->base_ci));
  lua_pushnumber(L, (unsigned long)&a);
  return 5;
}


static int table_query (lua_State *L) {
  const Table *t;
  int i = luaL_opt_int(L, 2, -1);
  luaL_check_type(L, 1, LUA_TTABLE);
  t = hvalue(index(L, 1));
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
    luaA_pushobject(L, val(&t->node[i]));
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
      incr_top(L);
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

static int metatable (lua_State *L) {
  luaL_check_any(L, 1);
  if (lua_isnone(L, 2)) {
    if (lua_getmetatable(L, 1) == 0)
      lua_pushnil(L);
  }
  else {
    lua_settop(L, 2);
    luaL_check_type(L, 2, LUA_TTABLE);
    lua_setmetatable(L, 1);
  }
  return 1;
}

static int newuserdata (lua_State *L) {
  size_t size = luaL_check_int(L, 1);
  char *p = cast(char *, lua_newuserdata(L, size));
  while (size--) *p++ = '\0';
  return 1;
}


static int pushuserdata (lua_State *L) {
  lua_pushudataval(L, cast(void *, luaL_check_int(L, 1)));
  return 1;
}


static int udataval (lua_State *L) {
  lua_pushnumber(L, cast(int, lua_touserdata(L, 1)));
  return 1;
}


static int doonnewstack (lua_State *L) {
  lua_State *L1 = lua_newthread(L);
  int status = lua_dostring(L1, luaL_check_string(L, 1));
  lua_pushnumber(L, status);
  lua_closethread(L, L1);
  return 1;
}


static int s2d (lua_State *L) {
  lua_pushnumber(L, *cast(const double *, luaL_check_string(L, 1)));
  return 1;
}

static int d2s (lua_State *L) {
  double d = luaL_check_number(L, 1);
  lua_pushlstring(L, cast(char *, &d), sizeof(d));
  return 1;
}


static int newstate (lua_State *L) {
  lua_State *L1 = lua_open();
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
  const char *code = luaL_check_string(L, 2);
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
    while (!lua_isnone(L1, ++i))
      lua_pushstring(L, lua_tostring(L1, i));
    lua_pop(L1, i-1);
    return i-1;
  }
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

static const char *const delimits = " \t\n,;";

static void skip (const char **pc) {
  while (**pc != '\0' && strchr(delimits, **pc)) (*pc)++;
}

static int getnum_aux (lua_State *L, const char **pc) {
  int res = 0;
  int sig = 1;
  skip(pc);
  if (**pc == '.') {
    res = cast(int, lua_tonumber(L, -1));
    lua_pop(L, 1);
    (*pc)++;
    return res;
  }
  else if (**pc == '-') {
    sig = -1;
    (*pc)++;
  }
  while (isdigit(cast(int, **pc))) res = res*10 + (*(*pc)++) - '0';
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


#define EQ(s1)	(strcmp(s1, inst) == 0)

#define getnum	(getnum_aux(L, &pc))
#define getname	(getname_aux(buff, &pc))


static int testC (lua_State *L) {
  char buff[30];
  const char *pc = luaL_check_string(L, 1);
  for (;;) {
    const char *inst = getname;
    if EQ("") return 0;
    else if EQ("isnumber") {
      lua_pushnumber(L, lua_isnumber(L, getnum));
    }
    else if EQ("isstring") {
      lua_pushnumber(L, lua_isstring(L, getnum));
    }
    else if EQ("istable") {
      lua_pushnumber(L, lua_istable(L, getnum));
    }
    else if EQ("iscfunction") {
      lua_pushnumber(L, lua_iscfunction(L, getnum));
    }
    else if EQ("isfunction") {
      lua_pushnumber(L, lua_isfunction(L, getnum));
    }
    else if EQ("isuserdata") {
      lua_pushnumber(L, lua_isuserdata(L, getnum));
    }
    else if EQ("isnil") {
      lua_pushnumber(L, lua_isnil(L, getnum));
    }
    else if EQ("isnull") {
      lua_pushnumber(L, lua_isnone(L, getnum));
    }
    else if EQ("tonumber") {
      lua_pushnumber(L, lua_tonumber(L, getnum));
    }
    else if EQ("tostring") {
      const char *s = lua_tostring(L, getnum);
      lua_pushstring(L, s);
    }
    else if EQ("tonumber") {
      lua_pushnumber(L, lua_tonumber(L, getnum));
    }
    else if EQ("strlen") {
      lua_pushnumber(L, lua_strlen(L, getnum));
    }
    else if EQ("tocfunction") {
      lua_pushcfunction(L, lua_tocfunction(L, getnum));
    }
    else if EQ("return") {
      return getnum;
    }
    else if EQ("gettop") {
      lua_pushnumber(L, lua_gettop(L));
    }
    else if EQ("settop") {
      lua_settop(L, getnum);
    }
    else if EQ("pop") {
      lua_pop(L, getnum);
    }
    else if EQ("pushnum") {
      lua_pushnumber(L, getnum);
    }
    else if EQ("pushnil") {
      lua_pushnil(L);
    }
    else if EQ("pushbool") {
      lua_pushboolean(L, getnum);
    }
    else if EQ("tobool") {
      lua_pushnumber(L, lua_toboolean(L, getnum));
    }
    else if EQ("pushvalue") {
      lua_pushvalue(L, getnum);
    }
    else if EQ("pushcclosure") {
      lua_pushcclosure(L, testC, getnum);
    }
    else if EQ("pushupvalues") {
      lua_pushupvalues(L);
    }
    else if EQ("remove") {
      lua_remove(L, getnum);
    }
    else if EQ("insert") {
      lua_insert(L, getnum);
    }
    else if EQ("replace") {
      lua_replace(L, getnum);
    }
    else if EQ("gettable") {
      lua_gettable(L, getnum);
    }
    else if EQ("settable") {
      lua_settable(L, getnum);
    }
    else if EQ("next") {
      lua_next(L, -2);
    }
    else if EQ("concat") {
      lua_concat(L, getnum);
    }
    else if EQ("lessthan") {
      int a = getnum;
      lua_pushboolean(L, lua_lessthan(L, a, getnum));
    }
    else if EQ("equal") {
      int a = getnum;
      lua_pushboolean(L, lua_equal(L, a, getnum));
    }
    else if EQ("rawcall") {
      int narg = getnum;
      int nres = getnum;
      lua_rawcall(L, narg, nres);
    }
    else if EQ("call") {
      int narg = getnum;
      int nres = getnum;
      lua_call(L, narg, nres);
    }
    else if EQ("dostring") {
      lua_dostring(L, luaL_check_string(L, getnum));
    }
    else if EQ("setmetatable") {
      lua_setmetatable(L, getnum);
    }
    else if EQ("getmetatable") {
      if (lua_getmetatable(L, getnum) == 0)
        lua_pushnil(L);
    }
    else if EQ("type") {
      lua_pushstring(L, lua_typename(L, lua_type(L, getnum)));
    }
    else luaL_verror(L, "unknown instruction %.30s", buff);
  }
  return 0;
}

/* }====================================================== */



static const struct luaL_reg tests_funcs[] = {
  {"hash", hash_query},
  {"limits", get_limits},
  {"listcode", listcode},
  {"listk", listk},
  {"listlocals", listlocals},
  {"loadlib", loadlib},
  {"stacklevel", stacklevel},
  {"querystr", string_query},
  {"querytab", table_query},
  {"testC", testC},
  {"ref", tref},
  {"getref", getref},
  {"unref", unref},
  {"d2s", d2s},
  {"s2d", s2d},
  {"metatable", metatable},
  {"newuserdata", newuserdata},
  {"pushuserdata", pushuserdata},
  {"udataval", udataval},
  {"doonnewstack", doonnewstack},
  {"newstate", newstate},
  {"closestate", closestate},
  {"doremote", doremote},
  {"log2", log2_aux},
  {"totalmem", mem_query},
  {NULL, NULL}
};


static void fim (void) {
  if (!islocked)
    lua_close(lua_state);
  lua_assert(memdebug_numblocks == 0);
  lua_assert(memdebug_total == 0);
}


void luaB_opentests (lua_State *L) {
  *cast(int **, L) = &islocked;  /* init lock */
  lua_state = L;  /* keep first state to be opened */
  luaL_opennamedlib(L, "T", tests_funcs, 0);
  atexit(fim);
}

#endif
