/*
** $Id: ltests.c,v 1.34 2000/08/15 20:14:27 roberto Exp roberto $
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


void luaB_opentests (lua_State *L);


/*
** The whole module only makes sense with DEBUG on
*/
#ifdef DEBUG



static void setnameval (lua_State *L, const char *name, int val) {
  lua_pushobject(L, -1);
  lua_pushstring(L, name);
  lua_pushnumber(L, val);
  lua_settable(L);
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
  lua_pushstring(L, buff);
  return (o != OP_END);
}


static int listcode (lua_State *L) {
  int pc;
  Proto *p;
  int res;
  luaL_arg_check(L, lua_tag(L, 1) == TAG_LCLOSURE, 1, "Lua function expected");
  p = clvalue(luaA_index(L, 1))->f.l;
  lua_newtable(L);
  setnameval(L, "maxstack", p->maxstacksize);
  setnameval(L, "numparams", p->numparams);
  pc = 0;
  do {
    lua_pushobject(L, -1);
    lua_pushnumber(L, pc+1);
    res = pushop(L, p, pc++);
    lua_settable(L);
  } while (res);
  return 1;
}


static int liststrings (lua_State *L) {
  Proto *p;
  int i;
  luaL_arg_check(L, lua_tag(L, 1) == TAG_LCLOSURE, 1, "Lua function expected");
  p = clvalue(luaA_index(L, 1))->f.l;
  lua_newtable(L);
  for (i=0; i<p->nkstr; i++) {
    lua_pushobject(L, -1);
    lua_pushnumber(L, i+1);
    lua_pushstring(L, p->kstr[i]->str);
    lua_settable(L);
  }
  return 1;
}


static int listlocals (lua_State *L) {
  Proto *p;
  int pc = luaL_check_int(L, 2) - 1;
  int i = 0;
  const char *name;
  luaL_arg_check(L, lua_tag(L, 1) == TAG_LCLOSURE, 1, "Lua function expected");
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
    luaL_arg_check(L, lua_tag(L, 1) == TAG_STRING, 1, "string expected");
    lua_pushnumber(L, tsvalue(luaA_index(L, 1))->u.s.hash);
  }
  else {
    Hash *t;
    luaL_checktype(L, 2, "table");
    t = hvalue(luaA_index(L, 2));
    lua_pushnumber(L, luaH_mainposition(t, luaA_index(L, 1)) - t->node);
  }
  return 1;
}


static int table_query (lua_State *L) {
  const Hash *t;
  int i = luaL_opt_int(L, 2, -1);
  luaL_checktype(L, 1, "table");
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
      ttype(L->top) = TAG_STRING;
      tsvalue(L->top) = ts;
      incr_top;
      n++;
    }
    return n;
  }
  return 0;
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

static int getnum (const char **pc, int *reg) {
  int res = 0;
  int sig = 1;
  int ref = 0;
  skip(pc);
  if (**pc == 'r') {
    ref = 1;
    (*pc)++;
  }
  else if (**pc == '-') {
    sig = -1;
    (*pc)++;
  }
  while (isdigit(**pc)) res = res*10 + (*(*pc)++) - '0';
  if (!ref)
    return sig*res;
  else
    return reg[res];
}
  
static int getreg (const char **pc) {
  skip(pc);
  (*pc)++;  /* skip the `r' */
  return getnum(pc, NULL);
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

#define getnum	((getnum)(&pc, reg))
#define getreg	((getreg)(&pc))
#define getname	((getname)(buff, &pc))


static int testC (lua_State *L) {
  char buff[30];
  int reg[10];
  const char *pc = luaL_check_string(L, 1);
  for (;;) {
    const char *inst = getname;
    if EQ("") return 0;
    else if EQ("return") {
      return getnum;
    }
    else if EQ("retall") {
      return lua_gettop(L) - 1;
    }
    else if EQ("gettop") {
      reg[getreg] = lua_gettop(L);
    }
    else if EQ("settop") {
      lua_settop(L, getnum);
    }
    else if EQ("setreg") {
      int n = getreg;
      reg[n] = lua_tonumber(L, getnum);
    }
    else if EQ("pushnum") {
      lua_pushnumber(L, getnum);
    }
    else if EQ("newtable") {
      lua_newtable(L);
    }
    else if EQ("closure") {
      lua_CFunction f;
      lua_getglobal(L, getname);
      f = lua_tocfunction(L, -1);
      lua_settop(L, -1);
      lua_pushcclosure(L, f, getnum);
    }
    else if EQ("pushobject") {
      lua_pushobject(L, getnum);
    }
    else if EQ("getglobal") {
      lua_getglobal(L, getname);
    }
    else if EQ("ref") {
      reg[getreg] = lua_ref(L, 0);
    }
    else if EQ("reflock") {
      reg[getreg] = lua_ref(L, 1);
    }
    else if EQ("getref") {
      int n = getreg;
      reg[n] = lua_getref(L, getnum);
    }
    else if EQ("unref") {
      lua_unref(L, getnum);
    }
    else if EQ("setglobal") {
      lua_setglobal(L, getname);
    }
    else if EQ("pushstring") {
      lua_pushstring(L, getname);
    }
    else if EQ("call") {
      int narg = getnum;
      int nres = getnum;
      if (lua_call(L, narg, nres)) lua_error(L, NULL);
    }
    else if EQ("gettable") {
      lua_gettable(L);
    }
    else if EQ("rawget") {
      lua_rawget(L);
    }
    else if EQ("settable") {
      lua_settable(L);
    }
    else if EQ("rawset") {
      lua_rawset(L);
    }
    else if EQ("tag") {
      int n = getreg;
      reg[n] = lua_tag(L, getnum);
    }
    else if EQ("type") {
      lua_pushstring(L, lua_type(L, getnum));
    }
    else if EQ("equal") {
      int n1 = getreg;
      int n2 = getnum;
      int n3 = getnum;
      reg[n1] = lua_equal(L, n2, n3);
    }
    else if EQ("pushusertag") {
      int val = getnum;
      int tag = getnum;
      lua_pushusertag(L, (void *)val, tag);
    }
    else if EQ("udataval") {
      int n = getreg;
      reg[n] = (int)lua_touserdata(L, getnum);
    }
    else if EQ("settagmethod") {
      int n = getnum;
      lua_settagmethod(L, n, getname);
    }
    else if EQ("newstate") {
      int stacksize = getnum;
      lua_State *L1 = lua_newstate(stacksize, getnum);
      if (L1)
        lua_pushuserdata(L, L1);
      else
        lua_pushnil(L);
    }
    else if EQ("closestate") {
      (lua_close)((lua_State *)lua_touserdata(L, getnum));
    }
    else if EQ("doremote") {
      int ol1 = getnum;
      int str = getnum;
      lua_State *L1;
      int status;
      if (!lua_isuserdata(L, ol1) || !lua_isstring(L, str))
        lua_error(L, "bad arguments for `doremote'");
      L1 = (lua_State *)lua_touserdata(L, ol1);
      status = lua_dostring(L1, lua_tostring(L, str));
      if (status != 0) {
        lua_pushnil(L);
        lua_pushnumber(L, status);
      }
      else {
        int i = 0;
        while (!lua_isnull(L, ++i))
          lua_pushstring(L, lua_tostring(L1, i));
      }
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
  {"querystr", string_query},
  {"querytab", table_query},
  {"testC", testC},
  {"totalmem", mem_query}
};


void luaB_opentests (lua_State *L) {
  luaL_openl(L, tests_funcs);
}

#endif
