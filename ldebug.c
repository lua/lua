/*
** $Id: ldebug.c,v 1.26 2000/06/30 14:29:35 roberto Exp roberto $
** Debug Interface
** See Copyright Notice in lua.h
*/


#include <stdlib.h>

#define LUA_REENTRANT

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


int lua_setdebug (lua_State *L, int debug) {
  int old = L->debug;
  L->debug = debug;
  return old;
}


static StkId aux_stackedfunction (lua_State *L, int level, StkId top) {
  int i;
  for (i = (top-1)-L->stack; i>=0; i--) {
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
    int *lines = ci->func->f.l->lines;
    if (!lines) return -1;  /* no static debug information */
    else return lines[lua_currentpc(f)];
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


static int checkfunc (lua_State *L, TObject *o) {
  return luaO_equalObj(o, L->top);
}


static void lua_getobjname (lua_State *L, StkId f, lua_Debug *ar) {
  Hash *g = L->gt;
  int i;
  /* try to find a name for given function */
  setnormalized(L->top, f); /* to be used by `checkfunc' */
  for (i=0; i<=g->size; i++) {
    if (ttype(key(node(g,i))) == TAG_STRING && checkfunc(L, val(node(g,i)))) {
      ar->name = tsvalue(key(node(g,i)))->str;
      ar->namewhat = "global";
      return;
    }
  }
  /* not found: try tag methods */
  if ((ar->name = luaT_travtagmethods(L, checkfunc)) != NULL)
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
        lua_getobjname(L, func, ar);
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

static Instruction luaG_symbexec (const Proto *pt, int lastpc, int stackpos) {
  int stack[MAXSTACK];  /* stores last instruction that changes each value */
  const Instruction *code = pt->code;
  int top = pt->numparams;
  int pc = 0;
  if (pt->is_vararg)  /* varargs? */
    top++;  /* `arg' */
  while (pc < lastpc) {
    const Instruction i = code[pc++];
    LUA_ASSERT(top <= pt->maxstacksize, "wrong stack");
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
        top = GETARG_A(i);
        while (nresults--)
          stack[top++] = pc-1;
        break;
      }
      case OP_TAILCALL: {
        LUA_ASSERT(top >= GETARG_A(i), "wrong stack");
        top = GETARG_B(i);
        break;
      }
      case OP_PUSHNIL: {
        int n;
        for (n=0; n<GETARG_U(i); n++)
          stack[top++] = pc-1;
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
      case OP_JMPONT:
      case OP_JMPONF: {
        int newpc = pc + GETARG_S(i);
        if (lastpc < newpc)
          top--;  /* original code did not jump; condition was false */
        else {
          stack[top-1] = pc-1;  /* value generated by or-and */
          pc = newpc;  /* do the jump */
        }
        break;
      }
      case OP_PUSHNILJMP: {
        break;  /* do not `push', to compensate next instruction */
      }
      case OP_CLOSURE: {
        top -= GETARG_B(i);
        stack[top++] = pc-1;
        break;
      }
      default: {
        int n;
        LUA_ASSERT(luaK_opproperties[GET_OPCODE(i)].push != VD,
                   "invalid opcode for default");
        top -= luaK_opproperties[GET_OPCODE(i)].pop;
        LUA_ASSERT(top >= 0, "wrong stack");
        for (n=0; n<luaK_opproperties[GET_OPCODE(i)].push; n++)
          stack[top++] = pc-1;
      }
    }
  }
  return code[stack[stackpos]];
}


static const char *getname (lua_State *L, StkId obj, const char **name) {
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
        return (*name) ? "local" : NULL;
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


static void call_index_error (lua_State *L, StkId o, const char *op,
                              const char *tp) {
  const char *name;
  const char *kind = getname(L, o, &name);
  if (kind)
    luaL_verror(L, "%s `%s' is not a %s", kind, name, tp);
  else
    luaL_verror(L, "attempt to %.10s a %.10s value", op, lua_type(L, o));
}


void luaG_callerror (lua_State *L, StkId func) {
  call_index_error(L, func, "call", "function");
}


void luaG_indexerror (lua_State *L, StkId t) {
  call_index_error(L, t, "index", "table");
}
