/*
** $Id: ltests.c,v 1.32 2000/08/08 20:42:07 roberto Exp roberto $
** Internal Module for Debugging of the Lua Implementation
** See Copyright Notice in lua.h
*/


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LUA_SINGLESTATE

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


void luaB_opentests (lua_State *L);


/*
** The whole module only makes sense with DEBUG on
*/
#ifdef DEBUG



static void setnameval (lua_Object t, const char *name, int val) {
  lua_pushobject(t);
  lua_pushstring(name);
  lua_pushnumber(val);
  lua_settable();
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


static int pushop (Proto *p, int pc) {
  char buff[100];
  Instruction i = p->code[pc];
  OpCode o = GET_OPCODE(i);
  const char *name = instrname[o];
  sprintf(buff, "%5d - ", luaG_getline(p->lineinfo, pc, 1, NULL));
  switch ((enum Mode)luaK_opproperties[o].mode) {  
    case iO:
      sprintf(buff+8, "%s", name);
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
  lua_pushstring(buff);
  return (o != OP_END);
}


static void listcode (void) {
  lua_Object o = luaL_nonnullarg(1);
  lua_Object t = lua_createtable();
  int pc;
  Proto *p;
  int res;
  luaL_arg_check(ttype(o) == TAG_LCLOSURE, 1, "Lua function expected");
  p = clvalue(o)->f.l;
  setnameval(t, "maxstack", p->maxstacksize);
  setnameval(t, "numparams", p->numparams);
  pc = 0;
  do {
    lua_pushobject(t);
    lua_pushnumber(pc+1);
    res = pushop(p, pc++);
    lua_settable();
  } while (res);
  lua_pushobject(t);
}


static void liststrings (void) {
  lua_Object o = luaL_nonnullarg(1);
  lua_Object t = lua_createtable();
  Proto *p;
  int i;
  luaL_arg_check(ttype(o) == TAG_LCLOSURE, 1, "Lua function expected");
  p = clvalue(o)->f.l;
  for (i=0; i<p->nkstr; i++) {
    lua_pushobject(t);
    lua_pushnumber(i+1);
    lua_pushstring(p->kstr[i]->str);
    lua_settable();
  }
  lua_pushobject(t);
}


static void listlocals (void) {
  lua_Object o = luaL_nonnullarg(1);
  Proto *p;
  int pc = luaL_check_int(2) - 1;
  int i = 1;
  const char *name;
  luaL_arg_check(ttype(o) == TAG_LCLOSURE, 1, "Lua function expected");
  p = clvalue(o)->f.l;
  while ((name = luaF_getlocalname(p, i++, pc)) != NULL)
    lua_pushstring(name);
}

/* }====================================================== */



static void get_limits (void) {
  lua_Object t = lua_createtable();
  setnameval(t, "BITS_INT", BITS_INT);
  setnameval(t, "LFPF", LFIELDS_PER_FLUSH);
  setnameval(t, "MAXARG_A", MAXARG_A);
  setnameval(t, "MAXARG_B", MAXARG_B);
  setnameval(t, "MAXARG_S", MAXARG_S);
  setnameval(t, "MAXARG_U", MAXARG_U);
  setnameval(t, "MAXLOCALS", MAXLOCALS);
  setnameval(t, "MAXPARAMS", MAXPARAMS);
  setnameval(t, "MAXSTACK", MAXSTACK);
  setnameval(t, "MAXUPVALUES", MAXUPVALUES);
  setnameval(t, "MAXVARSLH", MAXVARSLH);
  setnameval(t, "RFPF", RFIELDS_PER_FLUSH);
  setnameval(t, "SIZE_A", SIZE_A);
  setnameval(t, "SIZE_B", SIZE_B);
  setnameval(t, "SIZE_OP", SIZE_OP);
  setnameval(t, "SIZE_U", SIZE_U);
  lua_pushobject(t);
}


static void mem_query (void) {
  lua_Object arg = lua_getparam(1);
  if (arg == LUA_NOOBJECT) {
    lua_pushnumber(memdebug_total);
    lua_pushnumber(memdebug_numblocks);
    lua_pushnumber(memdebug_maxmem);
  }
  else
    memdebug_memlimit = luaL_check_int(1);
}


static void hash_query (void) {
  lua_Object o = luaL_nonnullarg(1);
  if (lua_getparam(2) == LUA_NOOBJECT) {
    luaL_arg_check(ttype(o) == TAG_STRING, 1, "string expected");
    lua_pushnumber(tsvalue(o)->u.s.hash);
  }
  else {
    const Hash *t = hvalue(luaL_tablearg(2));
    lua_pushnumber(luaH_mainposition(t, o) - t->node);
  }
}


static void table_query (void) {
  const Hash *t = hvalue(luaL_tablearg(1));
  int i = luaL_opt_int(2, -1);
  if (i == -1) {
    lua_pushnumber(t->size);
    lua_pushnumber(t->firstfree - t->node);
  }
  else if (i < t->size) {
    luaA_pushobject(lua_state, &t->node[i].key);
    luaA_pushobject(lua_state, &t->node[i].val);
    if (t->node[i].next)
      lua_pushnumber(t->node[i].next - t->node);
  }
}


static void string_query (void) {
  lua_State *L = lua_state;
  stringtable *tb = (*luaL_check_string(1) == 's') ? &L->strt : &L->udt;
  int s = luaL_opt_int(2, 0) - 1;
  if (s==-1) {
    lua_pushnumber(tb->nuse);
    lua_pushnumber(tb->size);
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
  
static int getreg (const char **pc) {
  skip(pc);
  if (*(*pc)++ != 'r') lua_error("`testC' expecting a register");
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


static void testC (void) {
  lua_Object reg[10];
  const char *pc = luaL_check_string(1);
  for (;;) {
    const char *inst = getname(&pc);
    if EQ("") return;
    else if EQ("pushnum") {
      lua_pushnumber(getnum(&pc));
    }
    else if EQ("createtable") {
      reg[getreg(&pc)] = lua_createtable();
    }
    else if EQ("closure") {
      lua_CFunction f = lua_getcfunction(lua_getglobal(getname(&pc)));
      lua_pushcclosure(f, getnum(&pc));
    }
    else if EQ("pop") {
      reg[getreg(&pc)] = lua_pop();
    }
    else if EQ("getglobal") {
      int n = getreg(&pc);
      reg[n] = lua_getglobal(getname(&pc));
    }
    else if EQ("ref") {
      lua_pushnumber(lua_ref(0));
      reg[getreg(&pc)] = lua_pop();
    }
    else if EQ("reflock") {
      lua_pushnumber(lua_ref(1));
      reg[getreg(&pc)] = lua_pop();
    }
    else if EQ("getref") {
      int n = getreg(&pc);
      reg[n] = lua_getref((int)lua_getnumber(reg[getreg(&pc)]));
    }
    else if EQ("unref") {
      lua_unref((int)lua_getnumber(reg[getreg(&pc)]));
    }
    else if EQ("getparam") {
      int n = getreg(&pc);
      reg[n] = lua_getparam(getnum(&pc)+1);  /* skips the command itself */
    }
    else if EQ("getresult") {
      int n = getreg(&pc);
      reg[n] = lua_getparam(getnum(&pc));
    }
    else if EQ("setglobal") {
      lua_setglobal(getname(&pc));
    }
    else if EQ("pushglobals") {
      lua_pushglobaltable();
    }
    else if EQ("pushstring") {
      lua_pushstring(getname(&pc));
    }
    else if EQ("pushreg") {
      lua_pushobject(reg[getreg(&pc)]);
    }
    else if EQ("call") {
      if (lua_call(getname(&pc))) lua_error(NULL);
    }
    else if EQ("gettable") {
      reg[getreg(&pc)] = lua_gettable();
    }
    else if EQ("rawget") {
      reg[getreg(&pc)] = lua_rawget();
    }
    else if EQ("settable") {
      lua_settable();
    }
    else if EQ("rawset") {
      lua_rawset();
    }
    else if EQ("tag") {
      lua_pushnumber(lua_tag(reg[getreg(&pc)]));
    }
    else if EQ("type") {
      lua_pushstring(lua_type(reg[getreg(&pc)]));
    }
    else if EQ("next") {
      int n = getreg(&pc);
      n = lua_next(reg[n], (int)lua_getnumber(reg[getreg(&pc)]));
      lua_pushnumber(n);
    }
    else if EQ("equal") {
      int n1 = getreg(&pc);
      int n2 = getreg(&pc);
      lua_pushnumber(lua_equal(reg[n1], reg[n2]));
    }
    else if EQ("pushusertag") {
      int val = getreg(&pc);
      int tag = getreg(&pc);
      lua_pushusertag((void *)(int)lua_getnumber(reg[val]),
                         (int)lua_getnumber(reg[tag]));
    }
    else if EQ("udataval") {
      int n = getreg(&pc);
      lua_pushnumber((int)lua_getuserdata(reg[getreg(&pc)]));
      reg[n] = lua_pop();
    }
    else if EQ("settagmethod") {
      int n = getreg(&pc);
      lua_settagmethod((int)lua_getnumber(reg[n]), getname(&pc));
    }
    else if EQ("beginblock") {
      lua_beginblock();
    }
    else if EQ("endblock") {
      lua_endblock();
    }
    else if EQ("newstate") {
      int stacksize = getnum(&pc);
      lua_State *L1 = lua_newstate(stacksize, getnum(&pc));
      if (L1)
        lua_pushuserdata(L1);
      else
        lua_pushnil();
    }
    else if EQ("closestate") {
      (lua_close)((lua_State *)lua_getuserdata(reg[getreg(&pc)]));
    }
    else if EQ("doremote") {
      lua_Object ol1 = reg[getreg(&pc)];
      lua_Object str = reg[getreg(&pc)];
      lua_State *L1;
      lua_Object temp;
      int status;
      if (!lua_isuserdata(ol1) || !lua_isstring(str))
        lua_error("bad arguments for `doremote'");
      L1 = (lua_State *)lua_getuserdata(ol1);
      status = (lua_dostring)(L1, lua_getstring(str));
      if (status != 0) {
        lua_pushnil();
        lua_pushnumber(status);
      }
      else {
        int i = 1;
        while ((temp = (lua_getresult)(L1, i++)) != LUA_NOOBJECT)
          lua_pushstring((lua_getstring)(L1, temp));
      }
    }
#if LUA_DEPRECATETFUNCS
    else if EQ("rawsetglobal") {
      lua_rawsetglobal(getname(&pc));
    }
    else if EQ("rawgetglobal") {
      int n = getreg(&pc);
      reg[n] = lua_rawgetglobal(getname(&pc));
    }
#endif
    else luaL_verror(lua_state, "unknown command in `testC': %.20s", inst);
  }
}

/* }====================================================== */



static const struct luaL_reg tests_funcs[] = {
  {"hash", (lua_CFunction)hash_query},
  {"limits", (lua_CFunction)get_limits},
  {"listcode", (lua_CFunction)listcode},
  {"liststrings", (lua_CFunction)liststrings},
  {"listlocals", (lua_CFunction)listlocals},
  {"querystr", (lua_CFunction)string_query},
  {"querytab", (lua_CFunction)table_query},
  {"testC", (lua_CFunction)testC},
  {"totalmem", (lua_CFunction)mem_query}
};


void luaB_opentests (lua_State *L) {
  if (lua_state != NULL) return;  /* do not open tests for auxiliar states */
  lua_state = L;
  luaL_openl(tests_funcs);
}

#endif
