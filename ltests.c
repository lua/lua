/*
** $Id: ltests.c,v 1.8 2000/02/21 18:30:06 roberto Exp roberto $
** Internal Module for Debugging of the Lua Implementation
** See Copyright Notice in lua.h
*/


#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define LUA_REENTRANT

#include "lapi.h"
#include "lauxlib.h"
#include "lmem.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "lua.h"
#include "luadebug.h"


void luaB_opentests (lua_State *L);


/*
** The whole module only makes sense with DEBUG on
*/
#ifdef DEBUG


static void mem_query (lua_State *L) {
  lua_pushnumber(L, memdebug_total);
  lua_pushnumber(L, memdebug_numblocks);
}


static void hash_query (lua_State *L) {
  lua_Object o = luaL_nonnullarg(L, 1);
  if (lua_getparam(L, 2) == LUA_NOOBJECT) {
    luaL_arg_check(L, ttype(o) == TAG_STRING, 1, "string expected");
    lua_pushnumber(L, tsvalue(o)->hash);
  }
  else {
    const Hash *t = avalue(luaL_tablearg(L, 2));
    lua_pushnumber(L, luaH_mainposition(t, o) - t->node);
  }
}


static void table_query (lua_State *L) {
  const Hash *t = avalue(luaL_tablearg(L, 1));
  int i = luaL_opt_int(L, 2, -1);
  if (i == -1) {
    lua_pushnumber(L, t->size);
    lua_pushnumber(L, t->firstfree - t->node);
  }
  else if (i < t->size) {
    luaA_pushobject(L, &t->node[i].key);
    luaA_pushobject(L, &t->node[i].val);
    if (t->node[i].next)
      lua_pushnumber(L, t->node[i].next - t->node);
  }
}


static void query_strings (lua_State *L) {
  int h = luaL_check_int(L, 1) - 1;
  int s = luaL_opt_int(L, 2, 0) - 1;
  if (s==-1) {
    if (h < NUM_HASHS) {
      lua_pushnumber(L, L->string_root[h].nuse);
      lua_pushnumber(L, L->string_root[h].size);
    }
  }
  else {
    TString *ts = L->string_root[h].hash[s];
    for (ts = L->string_root[h].hash[s]; ts; ts = ts->nexthash) {
      if (ts->constindex == -1) lua_pushstring(L, "<USERDATA>");
      else lua_pushstring(L, ts->str);
    }
  }
}


static const char *delimits = " \t\n,;";

static void skip (const char **pc) {
  while (**pc != '\0' && strchr(delimits, **pc)) (*pc)++;
}

static int getnum (const char **pc) {
  int res = 0;
  skip(pc);
  while (isdigit(**pc)) res = res*10 + (*(*pc)++) - '0';
  return res;
}
  
static int getreg (lua_State *L, const char **pc) {
  skip(pc);
  if (*(*pc)++ != 'r') lua_error(L, "`testC' expecting a register");
  return getnum(pc);
}

static const char *getname (const char **pc) {
  static char buff[30];
  int i = 0;
  skip(pc);
  while (**pc != '\0' && !strchr(delimits, **pc))
    buff[i++] = *(*pc)++;
  buff[i] = '\0';
  return buff;
}


#define EQ(s1)	(strcmp(s1, inst) == 0)

/*
** function to test the API with C. It interprets a kind of "assembler"
** language with calls to the API, so the test can be driven by Lua code
*/
static void testC (lua_State *L) {
  lua_Object reg[10];
  const char *pc = luaL_check_string(L, 1);
  for (;;) {
    const char *inst = getname(&pc);
    if EQ("") return;
    else if EQ("pushnum") {
      lua_pushnumber(L, getnum(&pc));
    }
    else if EQ("createtable") {
      reg[getreg(L, &pc)] = lua_createtable(L);
    }
    else if EQ("closure") {
      lua_CFunction f = lua_getcfunction(L, lua_getglobal(L, getname(&pc)));
      lua_pushcclosure(L, f, getnum(&pc));
    }
    else if EQ("pop") {
      reg[getreg(L, &pc)] = lua_pop(L);
    }
    else if EQ("getglobal") {
      int n = getreg(L, &pc);
      reg[n] = lua_getglobal(L, getname(&pc));
    }
    else if EQ("rawgetglobal") {
      int n = getreg(L, &pc);
      reg[n] = lua_rawgetglobal(L, getname(&pc));
    }
    else if EQ("ref") {
      lua_pushnumber(L, lua_ref(L, 0));
      reg[getreg(L, &pc)] = lua_pop(L);
    }
    else if EQ("reflock") {
      lua_pushnumber(L, lua_ref(L, 1));
      reg[getreg(L, &pc)] = lua_pop(L);
    }
    else if EQ("getref") {
      int n = getreg(L, &pc);
      reg[n] = lua_getref(L, (int)lua_getnumber(L, reg[getreg(L, &pc)]));
    }
    else if EQ("unref") {
      lua_unref(L, (int)lua_getnumber(L, reg[getreg(L, &pc)]));
    }
    else if EQ("getparam") {
      int n = getreg(L, &pc);
      reg[n] = lua_getparam(L, getnum(&pc)+1);  /* skips the command itself */
    }
    else if EQ("getresult") {
      int n = getreg(L, &pc);
      reg[n] = lua_getparam(L, getnum(&pc));
    }
    else if EQ("setglobal") {
      lua_setglobal(L, getname(&pc));
    }
    else if EQ("rawsetglobal") {
      lua_rawsetglobal(L, getname(&pc));
    }
    else if EQ("pushstring") {
      lua_pushstring(L, getname(&pc));
    }
    else if EQ("pushreg") {
      lua_pushobject(L, reg[getreg(L, &pc)]);
    }
    else if EQ("call") {
      if (lua_call(L, getname(&pc))) lua_error(L, NULL);
    }
    else if EQ("gettable") {
      reg[getreg(L, &pc)] = lua_gettable(L);
    }
    else if EQ("rawgettable") {
      reg[getreg(L, &pc)] = lua_rawgettable(L);
    }
    else if EQ("settable") {
      lua_settable(L);
    }
    else if EQ("rawsettable") {
      lua_rawsettable(L);
    }
    else if EQ("tag") {
      lua_pushnumber(L, lua_tag(L, reg[getreg(L, &pc)]));
    }
    else if EQ("type") {
      lua_pushstring(L, lua_type(L, reg[getreg(L, &pc)]));
    }
    else if EQ("nextvar") {
      lua_pushstring(L, lua_nextvar(L, lua_getstring(L, reg[getreg(L, &pc)])));
    }
    else if EQ("next") {
      int n = getreg(L, &pc);
      n = lua_next(L, reg[n], (int)lua_getnumber(L, reg[getreg(L, &pc)]));
      lua_pushnumber(L, n);
    }
    else if EQ("equal") {
      int n1 = getreg(L, &pc);
      int n2 = getreg(L, &pc);
      lua_pushnumber(L, lua_equal(L, reg[n1], reg[n2]));
    }
    else if EQ("pushusertag") {
      int val = getreg(L, &pc);
      int tag = getreg(L, &pc);
      lua_pushusertag(L, (void *)(int)lua_getnumber(L, reg[val]),
                         (int)lua_getnumber(L, reg[tag]));
    }
    else if EQ("udataval") {
      int n = getreg(L, &pc);
      lua_pushnumber(L, (int)lua_getuserdata(L, reg[getreg(L, &pc)]));
      reg[n] = lua_pop(L);
    }
    else if EQ("settagmethod") {
      int n = getreg(L, &pc);
      lua_settagmethod(L, (int)lua_getnumber(L, reg[n]), getname(&pc));
    }
    else if EQ("beginblock") {
      lua_beginblock(L);
    }
    else if EQ("endblock") {
      lua_endblock(L);
    }
    else if EQ("newstate") {
      int stacksize = getnum(&pc);
      lua_State *L1 = lua_newstate("stack", stacksize,
                                   "builtin", getnum(&pc), NULL);
      lua_pushuserdata(L, L1);
    }
    else if EQ("closestate") {
      lua_close((lua_State *)lua_getuserdata(L, reg[getreg(L, &pc)]));
    }
    else if EQ("doremote") {
      lua_Object ol1 = reg[getreg(L, &pc)];
      lua_Object str = reg[getreg(L, &pc)];
      lua_State *L1;
      lua_Object temp;
      int i;
      if (!lua_isuserdata(L, ol1) || !lua_isstring(L, str))
        lua_error(L, "bad arguments for `doremote'");
      L1 = (lua_State *)lua_getuserdata(L, ol1);
      lua_dostring(L1, lua_getstring(L, str));
      i = 1;
      while ((temp = lua_getresult(L1, i++)) != LUA_NOOBJECT)
        lua_pushstring(L, lua_getstring(L1, temp));
    }
    else luaL_verror(L, "unknown command in `testC': %.20s", inst);
  }
}


static const struct luaL_reg tests_funcs[] = {
  {"hash", hash_query},
  {"querystr", query_strings},
  {"querytab", table_query},
  {"testC", testC},
  {"totalmem", mem_query}
};


void luaB_opentests (lua_State *L) {
  luaL_openl(L, tests_funcs);
}

#endif
