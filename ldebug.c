/*
** $Id: ldebug.c,v 1.31 2000/08/09 19:16:57 roberto Exp roberto $
** Debug Interface
** See Copyright Notice in lua.h
*/


#include <stdlib.h>

#include "lua.h"

#include "lapi.h"
#include "lauxlib.h"
#include "lcode.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lstate.h"
#include "ltable.h"
#include "ltm.h"
#include "luadebug.h"



static void setnormalized (TObject *d, const TObject *s) {
  switch (s->ttype) {
    case TAG_CMARK: {
      clvalue(d) = clvalue(s);
      ttype(d) = TAG_CCLOSURE;
      break;
    }
    case TAG_LMARK: {
      clvalue(d) = infovalue(s)->func;
      ttype(d) = TAG_LCLOSURE;
      break;
    }
    default: *d = *s;
  }
}


lua_Hook lua_setcallhook (lua_State *L, lua_Hook func) {
  lua_Hook oldhook = L->callhook;
  L->callhook = func;
  return oldhook;
}


lua_Hook lua_setlinehook (lua_State *L, lua_Hook func) {
  lua_Hook oldhook = L->linehook;
  L->linehook = func;
  return oldhook;
}


static StkId aux_stackedfunction (lua_State *L, int level, StkId top) {
  int i;
  for (i = (top-1) - L->stack; i>=0; i--) {
    if (is_T_MARK(L->stack[i].ttype)) {
      if (level == 0)
        return L->stack+i;
      level--;
    }
  }
  return NULL;
}


int lua_getstack (lua_State *L, int level, lua_Debug *ar) {
  StkId f = aux_stackedfunction(L, level, L->top);
  if (f == NULL) return 0;  /* there is no such level */
  else {
    ar->_func = f;
    return 1;
  }
}


static int lua_nups (StkId f) {
  switch (ttype(f)) {
    case TAG_LCLOSURE:  case TAG_CCLOSURE: case TAG_CMARK:
      return clvalue(f)->nupvalues;
    case TAG_LMARK:
      return infovalue(f)->func->nupvalues;
    default:
      return 0;
  }
}


int luaG_getline (int *lineinfo, int pc, int refline, int *prefi) {
  int refi = prefi ? *prefi : 0;
  if (lineinfo[refi] < 0)
    refline += -lineinfo[refi++]; 
  LUA_ASSERT(lineinfo[refi] >= 0, "invalid line info");
  while (lineinfo[refi] > pc) {
    refline--;
    refi--;
    if (lineinfo[refi] < 0)
      refline -= -lineinfo[refi--]; 
    LUA_ASSERT(lineinfo[refi] >= 0, "invalid line info");
  }
  for (;;) {
    int nextline = refline + 1;
    int nextref = refi + 1;
    if (lineinfo[nextref] < 0)
      nextline += -lineinfo[nextref++]; 
    LUA_ASSERT(lineinfo[nextref] >= 0, "invalid line info");
    if (lineinfo[nextref] > pc)
      break;
    refline = nextline;
    refi = nextref;
  }
  if (prefi) *prefi = refi;
  return refline;
}


static int lua_currentpc (StkId f) {
  CallInfo *ci = infovalue(f);
  LUA_ASSERT(ttype(f) == TAG_LMARK, "function has no pc");
  return (*ci->pc - 1) - ci->func->f.l->code;
}


static int lua_currentline (StkId f) {
  if (ttype(f) != TAG_LMARK)
    return -1;  /* only active lua functions have current-line information */
  else {
    CallInfo *ci = infovalue(f);
    int *lineinfo = ci->func->f.l->lineinfo;
    return luaG_getline(lineinfo, lua_currentpc(f), 1, NULL);
  }
}



static Proto *getluaproto (StkId f) {
  return (ttype(f) == TAG_LMARK) ?  infovalue(f)->func->f.l : NULL;
}


int lua_getlocal (lua_State *L, const lua_Debug *ar, lua_Localvar *v) {
  StkId f = ar->_func;
  Proto *fp = getluaproto(f);
  if (!fp) return 0;  /* `f' is not a Lua function? */
  v->name = luaF_getlocalname(fp, v->index, lua_currentpc(f));
  if (!v->name) return 0;
  v->value = luaA_putluaObject(L, (f+1)+(v->index-1));
  return 1;
}


int lua_setlocal (lua_State *L, const lua_Debug *ar, lua_Localvar *v) {
  StkId f = ar->_func;
  Proto *fp = getluaproto(f);
  UNUSED(L);
  if (!fp) return 0;  /* `f' is not a Lua function? */
  v->name = luaF_getlocalname(fp, v->index, lua_currentpc(f));
  if (!v->name || v->name[0] == '*') return 0;  /* `*' starts private locals */
  *((f+1)+(v->index-1)) = *v->value;
  return 1;
}


static void lua_funcinfo (lua_Debug *ar, StkId func) {
  switch (ttype(func)) {
    case TAG_LCLOSURE:
      ar->source = clvalue(func)->f.l->source->str;
      ar->linedefined = clvalue(func)->f.l->lineDefined;
      ar->what = "Lua";
      break;
    case TAG_LMARK:
      ar->source = infovalue(func)->func->f.l->source->str;
      ar->linedefined = infovalue(func)->func->f.l->lineDefined;
      ar->what = "Lua";
      break;
    case TAG_CCLOSURE:  case TAG_CMARK:
      ar->source = "(C)";
      ar->linedefined = -1;
      ar->what = "C";
      break;
    default:
      LUA_INTERNALERROR("invalid `func' value");
  }
  if (ar->linedefined == 0)
    ar->what = "main";
}


static const char *travtagmethods (lua_State *L, const TObject *o) {
  int e;
  for (e=0; e<IM_N; e++) {
    int t;
    for (t=0; t<=L->last_tag; t++)
      if (luaO_equalObj(o, luaT_getim(L, t,e)))
        return luaT_eventname[e];
  }
  return NULL;
}


static const char *travglobals (lua_State *L, const TObject *o) {
  Hash *g = L->gt;
  int i;
  for (i=0; i<=g->size; i++) {
    if (luaO_equalObj(o, val(node(g, i))) &&
        ttype(key(node(g, i))) == TAG_STRING) 
      return tsvalue(key(node(g, i)))->str;
  }
  return NULL;
}


static void lua_getname (lua_State *L, StkId f, lua_Debug *ar) {
  TObject o;
  setnormalized(&o, f);
  /* try to find a name for given function */
  if ((ar->name = travglobals(L, &o)) != NULL)
    ar->namewhat = "global";
  /* not found: try tag methods */
  else if ((ar->name = travtagmethods(L, &o)) != NULL)
    ar->namewhat = "tag-method";
  else ar->namewhat = "";  /* not found at all */
}


int lua_getinfo (lua_State *L, const char *what, lua_Debug *ar) {
  StkId func;
  if (*what != '>')
    func = ar->_func;
  else {
    what++;  /* skip the '>' */
    func = ar->func;
  }
  for (; *what; what++) {
    switch (*what) {
      case 'S':
        lua_funcinfo(ar, func);
        break;
      case 'l':
        ar->currentline = lua_currentline(func);
        break;
      case 'u':
        ar->nups = lua_nups(func);
        break;
      case 'n':
        lua_getname(L, func, ar);
        break;
      case 'f':
        setnormalized(L->top, func);
        incr_top;
        ar->func = lua_pop(L);
        break;
      default: return 0;  /* invalid option */
    }
  }
  return 1;
}


/*
** {======================================================
** Symbolic Execution
** =======================================================
*/


static int pushpc (int *stack, int pc, int top, int n) {
  while (n--)
    stack[top++] = pc-1;
  return top;
}


static Instruction luaG_symbexec (const Proto *pt, int lastpc, int stackpos) {
  int stack[MAXSTACK];  /* stores last instruction that changed a stack entry */
  const Instruction *code = pt->code;
  int top = pt->numparams;
  int pc = 0;
  if (pt->is_vararg)  /* varargs? */
    top++;  /* `arg' */
  while (pc < lastpc) {
    const Instruction i = code[pc++];
    LUA_ASSERT(0 <= top && top <= pt->maxstacksize, "wrong stack");
    switch (GET_OPCODE(i)) {
      case OP_RETURN: {
        LUA_ASSERT(top >= GETARG_U(i), "wrong stack");
        top = GETARG_U(i);
        break;
      }
      case OP_CALL: {
        int nresults = GETARG_B(i);
        if (nresults == MULT_RET) nresults = 1;
        LUA_ASSERT(top >= GETARG_A(i), "wrong stack");
        top = pushpc(stack, pc, GETARG_A(i), nresults);
        break;
      }
      case OP_TAILCALL: {
        LUA_ASSERT(top >= GETARG_A(i), "wrong stack");
        top = GETARG_B(i);
        break;
      }
      case OP_PUSHNIL: {
        top = pushpc(stack, pc, top, GETARG_U(i));
        break;
      }
      case OP_POP: {
        top -= GETARG_U(i);
        break;
      }
      case OP_SETTABLE:
      case OP_SETLIST: {
        top -= GETARG_B(i);
        break;
      }
      case OP_SETMAP: {
        top -= 2*GETARG_U(i);
        break;
      }
      case OP_CONCAT: {
        top -= GETARG_U(i);
        stack[top++] = pc-1;
        break;
      }
      case OP_CLOSURE: {
        top -= GETARG_B(i);
        stack[top++] = pc-1;
        break;
      }
      default: {
        OpCode op = GET_OPCODE(i);
        LUA_ASSERT(luaK_opproperties[op].push != VD,
                   "invalid opcode for default");
        top -= luaK_opproperties[op].pop;
        LUA_ASSERT(top >= 0, "wrong stack");
        top = pushpc(stack, pc, top, luaK_opproperties[op].push);
        if (ISJUMP(op)) {
          int newpc = pc + GETARG_S(i);
          /* jump is forward and do not skip `lastpc'? */
          if (pc < newpc && newpc <= lastpc) {
            if (op == OP_JMPONT || op == OP_JMPONF)
              stack[top++] = pc-1;  /* do not pop when jumping */
            pc = newpc;  /* do the jump */
          }
        }
      }
    }
  }
  return code[stack[stackpos]];
}


static const char *getobjname (lua_State *L, StkId obj, const char **name) {
  StkId func = aux_stackedfunction(L, 0, obj);
  if (func == NULL || ttype(func) != TAG_LMARK)
    return NULL;  /* not a Lua function */
  else {
    Proto *p = infovalue(func)->func->f.l;
    int pc = lua_currentpc(func);
    int stackpos = obj - (func+1);  /* func+1 == function base */
    Instruction i = luaG_symbexec(p, pc, stackpos);
    switch (GET_OPCODE(i)) {
      case OP_GETGLOBAL: {
        *name = p->kstr[GETARG_U(i)]->str;
        return "global";
      }
      case OP_GETLOCAL: {
        *name = luaF_getlocalname(p, GETARG_U(i)+1, pc);
        LUA_ASSERT(*name, "local must exist");
        return "local";
      }
      case OP_PUSHSELF:
      case OP_GETDOTTED: {
        *name = p->kstr[GETARG_U(i)]->str;
        return "field";
      }
      default:
        return NULL;  /* no usefull name found */
    }
  }
}


/* }====================================================== */


void luaG_typeerror (lua_State *L, StkId o, const char *op) {
  const char *name;
  const char *kind = getobjname(L, o, &name);
  const char *t = lua_type(L, o);
  if (kind)
    luaL_verror(L, "attempt to %.30s %.20s `%.40s' (a %.10s value)",
                op, kind, name, t);
  else
    luaL_verror(L, "attempt to %.30s a %.10s value", op, t);
}


void luaG_binerror (lua_State *L, StkId p1, lua_Type t, const char *op) {
  if (ttype(p1) == t) p1++;
  LUA_ASSERT(ttype(p1) != t, "must be an error");
  luaG_typeerror(L, p1, op);
}

