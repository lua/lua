/*
** $Id: ldebug.c,v 1.53 2001/01/18 15:59:09 roberto Exp roberto $
** Debug Interface
** See Copyright Notice in lua.h
*/


#include <stdlib.h>

#include "lua.h"

#include "lapi.h"
#include "lcode.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "luadebug.h"



static const char *getfuncname (lua_State *L, StkId f, const char **name);


static void setnormalized (TObject *d, const TObject *s) {
  if (ttype(s) == LUA_TMARK) {
    setclvalue(d, infovalue(s)->func);
  }
  else setobj(d, s);
}


static int isLmark (StkId o) {
  return (o && ttype(o) == LUA_TMARK && !infovalue(o)->func->isC);
}


LUA_API lua_Hook lua_setcallhook (lua_State *L, lua_Hook func) {
  lua_Hook oldhook = L->callhook;
  L->callhook = func;
  return oldhook;
}


LUA_API lua_Hook lua_setlinehook (lua_State *L, lua_Hook func) {
  lua_Hook oldhook = L->linehook;
  L->linehook = func;
  return oldhook;
}


static StkId aux_stackedfunction (lua_State *L, int level, StkId top) {
  int i;
  for (i = (top-1) - L->stack; i>=0; i--) {
    if (is_T_MARK(&L->stack[i])) {
      if (level == 0)
        return L->stack+i;
      level--;
    }
  }
  return NULL;
}


LUA_API int lua_getstack (lua_State *L, int level, lua_Debug *ar) {
  StkId f = aux_stackedfunction(L, level, L->top);
  if (f == NULL) return 0;  /* there is no such level */
  else {
    ar->_func = f;
    return 1;
  }
}


static int nups (StkId f) {
  switch (ttype(f)) {
    case LUA_TFUNCTION:
      return clvalue(f)->nupvalues;
    case LUA_TMARK:
      return infovalue(f)->func->nupvalues;
    default:
      return 0;
  }
}


int luaG_getline (int *lineinfo, int pc, int refline, int *prefi) {
  int refi;
  if (lineinfo == NULL || pc == -1)
    return -1;  /* no line info or function is not active */
  refi = prefi ? *prefi : 0;
  if (lineinfo[refi] < 0)
    refline += -lineinfo[refi++]; 
  lua_assert(lineinfo[refi] >= 0);
  while (lineinfo[refi] > pc) {
    refline--;
    refi--;
    if (lineinfo[refi] < 0)
      refline -= -lineinfo[refi--]; 
    lua_assert(lineinfo[refi] >= 0);
  }
  for (;;) {
    int nextline = refline + 1;
    int nextref = refi + 1;
    if (lineinfo[nextref] < 0)
      nextline += -lineinfo[nextref++]; 
    lua_assert(lineinfo[nextref] >= 0);
    if (lineinfo[nextref] > pc)
      break;
    refline = nextline;
    refi = nextref;
  }
  if (prefi) *prefi = refi;
  return refline;
}


static int currentpc (StkId f) {
  CallInfo *ci = infovalue(f);
  lua_assert(isLmark(f));
  if (ci->pc)
    return (*ci->pc - ci->func->f.l->code) - 1;
  else
    return -1;  /* function is not active */
}


static int currentline (StkId f) {
  if (!isLmark(f))
    return -1;  /* only active lua functions have current-line information */
  else {
    CallInfo *ci = infovalue(f);
    int *lineinfo = ci->func->f.l->lineinfo;
    return luaG_getline(lineinfo, currentpc(f), 1, NULL);
  }
}



static Proto *getluaproto (StkId f) {
  return (isLmark(f) ?  infovalue(f)->func->f.l : NULL);
}


LUA_API const char *lua_getlocal (lua_State *L, const lua_Debug *ar, int n) {
  const char *name;
  StkId f = ar->_func;
  Proto *fp = getluaproto(f);
  if (!fp) return NULL;  /* `f' is not a Lua function? */
  name = luaF_getlocalname(fp, n, currentpc(f));
  if (!name) return NULL;
  luaA_pushobject(L, (f+1)+(n-1));  /* push value */
  return name;
}


LUA_API const char *lua_setlocal (lua_State *L, const lua_Debug *ar, int n) {
  const char *name;
  StkId f = ar->_func;
  Proto *fp = getluaproto(f);
  L->top--;  /* pop new value */
  if (!fp) return NULL;  /* `f' is not a Lua function? */
  name = luaF_getlocalname(fp, n, currentpc(f));
  if (!name || name[0] == '(') return NULL;  /* `(' starts private locals */
  setobj((f+1)+(n-1), L->top);
  return name;
}


static void infoLproto (lua_Debug *ar, Proto *f) {
  ar->source = f->source->str;
  ar->linedefined = f->lineDefined;
  ar->what = "Lua";
}


static void funcinfo (lua_State *L, lua_Debug *ar, StkId func) {
  Closure *cl = NULL;
  switch (ttype(func)) {
    case LUA_TFUNCTION:
      cl = clvalue(func);
      break;
    case LUA_TMARK:
      cl = infovalue(func)->func;
      break;
    default:
      lua_error(L, "value for `lua_getinfo' is not a function");
  }
  if (cl->isC) {
    ar->source = "=C";
    ar->linedefined = -1;
    ar->what = "C";
  }
  else
    infoLproto(ar, cl->f.l);
  luaO_chunkid(ar->short_src, ar->source, sizeof(ar->short_src));
  if (ar->linedefined == 0)
    ar->what = "main";
}


static const char *travtagmethods (global_State *G, const TObject *o) {
  if (ttype(o) == LUA_TFUNCTION) {
    int e;
    for (e=0; e<TM_N; e++) {
      int t;
      for (t=0; t<G->ntag; t++)
        if (clvalue(o) == luaT_gettm(G, t, e))
          return luaT_eventname[e];
    }
  }
  return NULL;
}


static const char *travglobals (lua_State *L, const TObject *o) {
  Hash *g = L->gt;
  int i;
  for (i=0; i<g->size; i++) {
    if (luaO_equalObj(o, val(node(g, i))) &&
        ttype(key(node(g, i))) == LUA_TSTRING) 
      return tsvalue(key(node(g, i)))->str;
  }
  return NULL;
}


static void getname (lua_State *L, StkId f, lua_Debug *ar) {
  TObject o;
  setnormalized(&o, f);
  /* try to find a name for given function */
  if ((ar->name = travglobals(L, &o)) != NULL)
    ar->namewhat = "global";
  /* not found: try tag methods */
  else if ((ar->name = travtagmethods(G(L), &o)) != NULL)
    ar->namewhat = "tag-method";
  else ar->namewhat = "";  /* not found at all */
}


LUA_API int lua_getinfo (lua_State *L, const char *what, lua_Debug *ar) {
  StkId func;
  int isactive = (*what != '>');
  if (isactive)
    func = ar->_func;
  else {
    what++;  /* skip the '>' */
    func = L->top - 1;
  }
  for (; *what; what++) {
    switch (*what) {
      case 'S': {
        funcinfo(L, ar, func);
        break;
      }
      case 'l': {
        ar->currentline = currentline(func);
        break;
      }
      case 'u': {
        ar->nups = nups(func);
        break;
      }
      case 'n': {
        ar->namewhat = (isactive) ? getfuncname(L, func, &ar->name) : NULL;
        if (ar->namewhat == NULL)
          getname(L, func, ar);
        break;
      }
      case 'f': {
        setnormalized(L->top, func);
        incr_top;  /* push function */
        break;
      }
      default: return 0;  /* invalid option */
    }
  }
  if (!isactive) L->top--;  /* pop function */
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
    lua_assert(0 <= top && top <= pt->maxstacksize);
    switch (GET_OPCODE(i)) {
      case OP_RETURN: {
        lua_assert(top >= GETARG_U(i));
        top = GETARG_U(i);
        break;
      }
      case OP_TAILCALL: {
        lua_assert(top >= GETARG_A(i));
        top = GETARG_B(i);
        break;
      }
      case OP_CALL: {
        int nresults = GETARG_B(i);
        if (nresults == MULT_RET) nresults = 1;
        lua_assert(top >= GETARG_A(i));
        top = pushpc(stack, pc, GETARG_A(i), nresults);
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
      case OP_JMPONT:
      case OP_JMPONF: {
        int newpc = pc + GETARG_S(i);
        /* jump is forward and do not skip `lastpc'? */
        if (pc < newpc && newpc <= lastpc) {
          stack[top-1] = pc-1;  /* value comes from `and'/`or' */
          pc = newpc;  /* do the jump */
        }
        else
          top--;  /* do not jump; pop value */
        break;
      }
      default: {
        OpCode op = GET_OPCODE(i);
        lua_assert(luaK_opproperties[op].push != VD);
        top -= (int)luaK_opproperties[op].pop;
        lua_assert(top >= 0);
        top = pushpc(stack, pc, top, luaK_opproperties[op].push);
      }
    }
  }
  return code[stack[stackpos]];
}


static const char *getobjname (lua_State *L, StkId obj, const char **name) {
  StkId func = aux_stackedfunction(L, 0, obj);
  if (!isLmark(func))
    return NULL;  /* not an active Lua function */
  else {
    Proto *p = infovalue(func)->func->f.l;
    int pc = currentpc(func);
    int stackpos = obj - (func+1);  /* func+1 == function base */
    Instruction i = luaG_symbexec(p, pc, stackpos);
    lua_assert(pc != -1);
    switch (GET_OPCODE(i)) {
      case OP_GETGLOBAL: {
        *name = p->kstr[GETARG_U(i)]->str;
        return "global";
      }
      case OP_GETLOCAL: {
        *name = luaF_getlocalname(p, GETARG_U(i)+1, pc);
        lua_assert(*name);
        return "local";
      }
      case OP_PUSHSELF:
      case OP_GETDOTTED: {
        *name = p->kstr[GETARG_U(i)]->str;
        return "field";
      }
      default:
        return NULL;  /* no useful name found */
    }
  }
}


static const char *getfuncname (lua_State *L, StkId f, const char **name) {
  StkId func = aux_stackedfunction(L, 0, f);  /* calling function */
  if (!isLmark(func))
    return NULL;  /* not an active Lua function */
  else {
    Proto *p = infovalue(func)->func->f.l;
    int pc = currentpc(func);
    Instruction i;
    if (pc == -1) return NULL;  /* function is not activated */
    i = p->code[pc];
    switch (GET_OPCODE(i)) {
      case OP_CALL: case OP_TAILCALL:
        return getobjname(L, (func+1)+GETARG_A(i), name);
      default:
        return NULL;  /* no useful name found */
    }
  }
}


/* }====================================================== */


void luaG_typeerror (lua_State *L, StkId o, const char *op) {
  const char *name;
  const char *kind = getobjname(L, o, &name);
  const char *t = luaO_typename(o);
  if (kind)
    luaO_verror(L, "attempt to %.30s %.20s `%.40s' (a %.10s value)",
                op, kind, name, t);
  else
    luaO_verror(L, "attempt to %.30s a %.10s value", op, t);
}


void luaG_binerror (lua_State *L, StkId p1, int t, const char *op) {
  if (ttype(p1) == t) p1++;
  lua_assert(ttype(p1) != t);
  luaG_typeerror(L, p1, op);
}


void luaG_ordererror (lua_State *L, StkId top) {
  const char *t1 = luaO_typename(top-2);
  const char *t2 = luaO_typename(top-1);
  if (t1[2] == t2[2])
    luaO_verror(L, "attempt to compare two %.10s values", t1);
  else
    luaO_verror(L, "attempt to compare %.10s with %.10s", t1, t2);
}

