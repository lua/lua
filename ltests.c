/*
** $Id: ltests.c,v 1.20 2000/05/22 18:44:46 roberto Exp roberto $
** Internal Module for Debugging of the Lua Implementation
** See Copyright Notice in lua.h
*/


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LUA_REENTRANT

#include "lapi.h"
#include "lauxlib.h"
#include "lcode.h"
#include "ldo.h"
#include "lmem.h"
#include "lopcodes.h"
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



static void setnameval (lua_State *L, lua_Object t, const char *name, int val) {
  lua_pushobject(L, t);
  lua_pushstring(L, name);
  lua_pushnumber(L, val);
  lua_settable(L);
}


/*
** {======================================================
** Disassembler
** =======================================================
*/


static const char *const instrname[OP_SETLINE+1] = {
  "END", "RETURN", "CALL", "TAILCALL", "PUSHNIL", "POP", "PUSHINT", 
  "PUSHSTRING", "PUSHNUM", "PUSHNEGNUM", "PUSHUPVALUE", "GETLOCAL", 
  "GETGLOBAL", "GETTABLE", "GETDOTTED", "GETINDEXED", "PUSHSELF", 
  "CREATETABLE", "SETLOCAL", "SETGLOBAL", "SETTABLE", "SETLIST", "SETMAP", 
  "ADD", "ADDI", "SUB", "MULT", "DIV", "POW", "CONCAT", "MINUS", "NOT", 
  "JMPNE", "JMPEQ", "JMPLT", "JMPLE", "JMPGT", "JMPGE", "JMPT", "JMPF", 
  "JMPONT", "JMPONF", "JMP", "PUSHNILJMP", "FORPREP", "FORLOOP", "LFORPREP", 
  "LFORLOOP", "CLOSURE", "SETLINE"
};


static int pushop (lua_State *L, Instruction i) {
  char buff[100];
  OpCode o = GET_OPCODE(i);
  const char *name = instrname[o];
  switch ((enum Mode)luaK_opproperties[o].mode) {  
    case iO:
      sprintf(buff, "%s", name);
      break;
    case iU:
      sprintf(buff, "%-12s%4u", name, GETARG_U(i));
      break;
    case iS:
      sprintf(buff, "%-12s%4d", name, GETARG_S(i));
      break;
    case iAB:
      sprintf(buff, "%-12s%4d %4d", name, GETARG_A(i), GETARG_B(i));
      break;
  }
  lua_pushstring(L, buff);
  return (o != OP_END);
}


static void listcode (lua_State *L) {
  lua_Object o = luaL_nonnullarg(L, 1);
  lua_Object t = lua_createtable(L);
  Instruction *pc;
  Proto *p;
  int res;
  luaL_arg_check(L, ttype(o) == TAG_LCLOSURE, 1, "Lua function expected");
  p = clvalue(o)->f.l;
  setnameval(L, t, "maxstack", p->maxstacksize);
  setnameval(L, t, "numparams", p->numparams);
  pc = p->code;
  do {
    lua_pushobject(L, t);
    lua_pushnumber(L, pc - p->code + 1);
    res = pushop(L, *pc++);
    lua_settable(L);
  } while (res);
  lua_pushobject(L, t);
}

/* }====================================================== */



static void get_limits (lua_State *L) {
  lua_Object t = lua_createtable(L);
  setnameval(L, t, "SIZE_OP", SIZE_OP);
  setnameval(L, t, "SIZE_U", SIZE_U);
  setnameval(L, t, "SIZE_A", SIZE_A);
  setnameval(L, t, "SIZE_B", SIZE_B);
  setnameval(L, t, "MAXARG_U", MAXARG_U);
  setnameval(L, t, "MAXARG_S", MAXARG_S);
  setnameval(L, t, "MAXARG_A", MAXARG_A);
  setnameval(L, t, "MAXARG_B", MAXARG_B);
  setnameval(L, t, "MAXSTACK", MAXSTACK);
  setnameval(L, t, "MAXLOCALS", MAXLOCALS);
  setnameval(L, t, "MAXUPVALUES", MAXUPVALUES);
  setnameval(L, t, "MAXVARSLH", MAXVARSLH);
  setnameval(L, t, "MAXPARAMS", MAXPARAMS);
  setnameval(L, t, "LFPF", LFIELDS_PER_FLUSH);
  setnameval(L, t, "RFPF", RFIELDS_PER_FLUSH);
  lua_pushobject(L, t);
}


static void mem_query (lua_State *L) {
  lua_pushnumber(L, memdebug_total);
  lua_pushnumber(L, memdebug_numblocks);
  lua_pushnumber(L, memdebug_maxmem);
}


static void hash_query (lua_State *L) {
  lua_Object o = luaL_nonnullarg(L, 1);
  if (lua_getparam(L, 2) == LUA_NOOBJECT) {
    luaL_arg_check(L, ttype(o) == TAG_STRING, 1, "string expected");
    lua_pushnumber(L, tsvalue(o)->u.s.hash);
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


static void string_query (lua_State *L) {
  stringtable *tb = (*luaL_check_string(L, 1) == 's') ? &L->strt : &L->udt;
  int s = luaL_opt_int(L, 2, 0) - 1;
  if (s==-1) {
    lua_pushnumber(L, tb->nuse);
    lua_pushnumber(L, tb->size);
  }
  else if (s < tb->size) {
    TString *ts;
    for (ts = tb->hash[s]; ts; ts = ts->nexthash) {
      ttype(L->top) = TAG_STRING;
      tsvalue(L->top) = ts;
      incr_top;
    }
  }
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
    else if EQ("pushglobals") {
      lua_pushglobaltable(L);
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
    else if EQ("rawget") {
      reg[getreg(L, &pc)] = lua_rawget(L);
    }
    else if EQ("settable") {
      lua_settable(L);
    }
    else if EQ("rawset") {
      lua_rawset(L);
    }
    else if EQ("tag") {
      lua_pushnumber(L, lua_tag(L, reg[getreg(L, &pc)]));
    }
    else if EQ("type") {
      lua_pushstring(L, lua_type(L, reg[getreg(L, &pc)]));
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
#if LUA_DEPRECATETFUNCS
    else if EQ("rawsetglobal") {
      lua_rawsetglobal(L, getname(&pc));
    }
    else if EQ("rawgetglobal") {
      int n = getreg(L, &pc);
      reg[n] = lua_rawgetglobal(L, getname(&pc));
    }
#endif
    else luaL_verror(L, "unknown command in `testC': %.20s", inst);
  }
}

/* }====================================================== */



static const struct luaL_reg tests_funcs[] = {
  {"hash", hash_query},
  {"limits", get_limits},
  {"listcode", listcode},
  {"querystr", string_query},
  {"querytab", table_query},
  {"testC", testC},
  {"totalmem", mem_query}
};


void luaB_opentests (lua_State *L) {
  luaL_openl(L, tests_funcs);
}

#endif
