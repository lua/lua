/*
** $Id: ltests.c,v 1.54 2000/10/31 13:10:24 roberto Exp $
** Internal Module for Debugging of the Lua Implementation
** See Copyright Notice in lua.h
*/


#include <ctype.h>
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


void luaB_opentests (lua_State *L);


/*
** The whole module only makes sense with LUA_DEBUG on
*/
#ifdef LUA_DEBUG



static void setnameval (lua_State *L, const char *name, int val) {
  lua_pushstring(L, name);
  lua_pushnumber(L, val);
  lua_settable(L, -3);
}


/*
** {======================================================
** Disassembler
** =======================================================
*/


static const char *const instrname[NUM_OPCODES] = {
  "END", "RETURN", "CALL", "TAILCALL", "PUSHNIL", "POP", "PUSHINT", 
  "PUSHSTRING", "PUSHNUM", "PUSHNEGNUM", "PUSHUPVALUE", "GETLOCAL", 
  "GETGLOBAL", "GETTABLE", "GETDOTTED", "GETINDEXED", "PUSHSELF", 
  "CREATETABLE", "SETLOCAL", "SETGLOBAL", "SETTABLE", "SETLIST", "SETMAP", 
  "ADD", "ADDI", "SUB", "MULT", "DIV", "POW", "CONCAT", "MINUS", "NOT", 
  "JMPNE", "JMPEQ", "JMPLT", "JMPLE", "JMPGT", "JMPGE", "JMPT", "JMPF", 
  "JMPONT", "JMPONF", "JMP", "PUSHNILJMP", "FORPREP", "FORLOOP", "LFORPREP", 
  "LFORLOOP", "CLOSURE"
};


static int pushop (lua_State *L, Proto *p, int pc) {
  char buff[100];
  Instruction i = p->code[pc];
  OpCode o = GET_OPCODE(i);
  const char *name = instrname[o];
  sprintf(buff, "%5d - ", luaG_getline(p->lineinfo, pc, 1, NULL));
  switch ((enum Mode)luaK_opproperties[o].mode) {  
    case iO:
      sprintf(buff+8, "%-12s", name);
      break;
    case iU:
      sprintf(buff+8, "%-12s%4u", name, GETARG_U(i));
      break;
    case iS:
      sprintf(buff+8, "%-12s%4d", name, GETARG_S(i));
      break;
    case iAB:
      sprintf(buff+8, "%-12s%4d %4d", name, GETARG_A(i), GETARG_B(i));
      break;
  }
  lua_pushstring(L, buff);
  return (o != OP_END);
}


static int listcode (lua_State *L) {
  int pc;
  Proto *p;
  int res;
  luaL_arg_check(L, lua_isfunction(L, 1) && !lua_iscfunction(L, 1),
                 1, "Lua function expected");
  p = clvalue(luaA_index(L, 1))->f.l;
  lua_newtable(L);
  setnameval(L, "maxstack", p->maxstacksize);
  setnameval(L, "numparams", p->numparams);
  pc = 0;
  do {
    lua_pushnumber(L, pc+1);
    res = pushop(L, p, pc++);
    lua_settable(L, -3);
  } while (res);
  return 1;
}


static int liststrings (lua_State *L) {
  Proto *p;
  int i;
  luaL_arg_check(L, lua_isfunction(L, 1) && !lua_iscfunction(L, 1),
                 1, "Lua function expected");
  p = clvalue(luaA_index(L, 1))->f.l;
  lua_newtable(L);
  for (i=0; i<p->nkstr; i++) {
    lua_pushnumber(L, i+1);
    lua_pushstring(L, p->kstr[i]->str);
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
  p = clvalue(luaA_index(L, 1))->f.l;
  while ((name = luaF_getlocalname(p, ++i, pc)) != NULL)
    lua_pushstring(L, name);
  return i-1;
}

/* }====================================================== */



static int get_limits (lua_State *L) {
  lua_newtable(L);
  setnameval(L, "BITS_INT", BITS_INT);
  setnameval(L, "LFPF", LFIELDS_PER_FLUSH);
  setnameval(L, "MAXARG_A", MAXARG_A);
  setnameval(L, "MAXARG_B", MAXARG_B);
  setnameval(L, "MAXARG_S", MAXARG_S);
  setnameval(L, "MAXARG_U", MAXARG_U);
  setnameval(L, "MAXLOCALS", MAXLOCALS);
  setnameval(L, "MAXPARAMS", MAXPARAMS);
  setnameval(L, "MAXSTACK", MAXSTACK);
  setnameval(L, "MAXUPVALUES", MAXUPVALUES);
  setnameval(L, "MAXVARSLH", MAXVARSLH);
  setnameval(L, "RFPF", RFIELDS_PER_FLUSH);
  setnameval(L, "SIZE_A", SIZE_A);
  setnameval(L, "SIZE_B", SIZE_B);
  setnameval(L, "SIZE_OP", SIZE_OP);
  setnameval(L, "SIZE_U", SIZE_U);
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
    luaL_arg_check(L, lua_tag(L, 1) == LUA_TSTRING, 1, "string expected");
    lua_pushnumber(L, tsvalue(luaA_index(L, 1))->u.s.hash);
  }
  else {
    Hash *t;
    luaL_checktype(L, 2, LUA_TTABLE);
    t = hvalue(luaA_index(L, 2));
    lua_pushnumber(L, luaH_mainposition(t, luaA_index(L, 1)) - t->node);
  }
  return 1;
}


static int table_query (lua_State *L) {
  const Hash *t;
  int i = luaL_opt_int(L, 2, -1);
  luaL_checktype(L, 1, LUA_TTABLE);
  t = hvalue(luaA_index(L, 1));
  if (i == -1) {
    lua_pushnumber(L, t->size);
    lua_pushnumber(L, t->firstfree - t->node);
    return 2;
  }
  else if (i < t->size) {
    luaA_pushobject(L, &t->node[i].key);
    luaA_pushobject(L, &t->node[i].val);
    if (t->node[i].next) {
      lua_pushnumber(L, t->node[i].next - t->node);
      return 3;
    }
    else
      return 2;
  }
  return 0;
}


static int string_query (lua_State *L) {
  stringtable *tb = (*luaL_check_string(L, 1) == 's') ? &L->strt : &L->udt;
  int s = luaL_opt_int(L, 2, 0) - 1;
  if (s==-1) {
    lua_pushnumber(L ,tb->nuse);
    lua_pushnumber(L ,tb->size);
    return 2;
  }
  else if (s < tb->size) {
    TString *ts;
    int n = 0;
    for (ts = tb->hash[s]; ts; ts = ts->nexthash) {
      ttype(L->top) = LUA_TSTRING;
      tsvalue(L->top) = ts;
      incr_top;
      n++;
    }
    return n;
  }
  return 0;
}


static int tref (lua_State *L) {
  luaL_checkany(L, 1);
  lua_pushvalue(L, 1);
  lua_pushnumber(L, lua_ref(L, luaL_opt_int(L, 2, 1)));
  return 1;
}

static int getref (lua_State *L) {
  if (lua_getref(L, luaL_check_int(L, 1)))
    return 1;
  else
    return 0;
}

static int unref (lua_State *L) {
  lua_unref(L, luaL_check_int(L, 1));
  return 0;
}

static int newuserdata (lua_State *L) {
  if (lua_isnumber(L, 2))
    lua_pushusertag(L, (void *)luaL_check_int(L, 1), luaL_check_int(L, 2));
  else
    lua_newuserdata(L, luaL_check_int(L, 1));
  return 1;
}

static int udataval (lua_State *L) {
  luaL_checktype(L, 1, LUA_TUSERDATA);
  lua_pushnumber(L, (int)lua_touserdata(L, 1));
  return 1;
}

static int newstate (lua_State *L) {
  lua_State *L1 = lua_open(luaL_check_int(L, 1));
  if (L1)
    lua_pushuserdata(L, L1);
  else
    lua_pushnil(L);
  return 1;
}

static int loadlib (lua_State *L) {
  lua_State *L1 = (lua_State *)lua_touserdata(L, 1);
  switch (*luaL_check_string(L, 2)) {
    case 'm': lua_mathlibopen(L1); break;
    case 's': lua_strlibopen(L1); break;
    case 'i': lua_iolibopen(L1); break;
    case 'd': lua_dblibopen(L1); break;
    case 'b': lua_baselibopen(L1); break;
    default: luaL_argerror(L, 2, "invalid option");
  }
  return 0;
}

static int closestate (lua_State *L) {
  luaL_checktype(L, 1, LUA_TUSERDATA);
  lua_close((lua_State *)lua_touserdata(L, 1));
  return 0;
}

static int doremote (lua_State *L) {
  lua_State *L1;
  const char *code = luaL_check_string(L, 2);
  int status;
  luaL_checktype(L, 1, LUA_TUSERDATA);
  L1 = (lua_State *)lua_touserdata(L, 1);
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
    return i-1;
  }
}

static int settagmethod (lua_State *L) {
  int tag = luaL_check_int(L, 1);
  const char *event = luaL_check_string(L, 2);
  luaL_checkany(L, 3);
  lua_gettagmethod(L, tag, event);
  lua_pushvalue(L, 3);
  lua_settagmethod(L, tag, event);
  return 1;
}

static int pushbool (lua_State *L, int b) {
  if (b) lua_pushnumber(L, 1);
  else lua_pushnil(L);
  return 1;
}

static int equal (lua_State *L) {
  return pushbool(L, lua_equal(L, 1, 2));
}

  

/*
** {======================================================
** function to test the API with C. It interprets a kind of "assembler"
** language with calls to the API, so the test can be driven by Lua code
** =======================================================
*/

static const char *const delimits = " \t\n,;";

static void skip (const char **pc) {
  while (**pc != '\0' && strchr(delimits, **pc)) (*pc)++;
}

static int getnum (lua_State *L, const char **pc) {
  int res = 0;
  int sig = 1;
  skip(pc);
  if (**pc == '.') {
    res = (int)lua_tonumber(L, -1);
    lua_pop(L, 1);
    (*pc)++;
    return res;
  }
  else if (**pc == '-') {
    sig = -1;
    (*pc)++;
  }
  while (isdigit(**pc)) res = res*10 + (*(*pc)++) - '0';
  return sig*res;
}
  
static const char *getname (char *buff, const char **pc) {
  int i = 0;
  skip(pc);
  while (**pc != '\0' && !strchr(delimits, **pc))
    buff[i++] = *(*pc)++;
  buff[i] = '\0';
  return buff;
}


#define EQ(s1)	(strcmp(s1, inst) == 0)

#define getnum	((getnum)(L, &pc))
#define getname	((getname)(buff, &pc))


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
      lua_pushnumber(L, lua_isnull(L, getnum));
    }
    else if EQ("tonumber") {
      lua_pushnumber(L, lua_tonumber(L, getnum));
    }
    else if EQ("tostring") {
      lua_pushstring(L, lua_tostring(L, getnum));
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
    else if EQ("pushvalue") {
      lua_pushvalue(L, getnum);
    }
    else if EQ("remove") {
      lua_remove(L, getnum);
    }
    else if EQ("insert") {
      lua_insert(L, getnum);
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
    else if EQ("settagmethod") {
      int tag = getnum;
      const char *event = getname;
      lua_settagmethod(L, tag, event);
    }
    else if EQ("gettagmethod") {
      int tag = getnum;
      const char *event = getname;
      lua_gettagmethod(L, tag, event);
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
  {"liststrings", liststrings},
  {"listlocals", listlocals},
  {"loadlib", loadlib},
  {"querystr", string_query},
  {"querytab", table_query},
  {"testC", testC},
  {"ref", tref},
  {"getref", getref},
  {"unref", unref},
  {"newuserdata", newuserdata},
  {"udataval", udataval},
  {"newstate", newstate},
  {"closestate", closestate},
  {"doremote", doremote},
  {"settagmethod", settagmethod},
  {"equal", equal},
  {"totalmem", mem_query}
};


void luaB_opentests (lua_State *L) {
  lua_newtable(L);
  lua_getglobals(L);
  lua_pushvalue(L, -2);
  lua_setglobals(L);
  luaL_openl(L, tests_funcs);  /* open functions inside new table */
  lua_setglobals(L);  /* restore old table of globals */
  lua_setglobal(L, "T");  /* set new table as global T */
}

#endif
