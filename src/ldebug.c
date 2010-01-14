/*
** $Id: ldebug.c,v 2.63 2010/01/13 16:18:25 roberto Exp $
** Debug Interface
** See Copyright Notice in lua.h
*/


#include <stdarg.h>
#include <stddef.h>
#include <string.h>


#define ldebug_c
#define LUA_CORE

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
#include "lvm.h"



static const char *getfuncname (lua_State *L, CallInfo *ci, const char **name);


static int currentpc (CallInfo *ci) {
  lua_assert(isLua(ci));
  return pcRel(ci->u.l.savedpc, ci_func(ci)->l.p);
}


static int currentline (CallInfo *ci) {
  return getfuncline(ci_func(ci)->l.p, currentpc(ci));
}


/*
** this function can be called asynchronous (e.g. during a signal)
*/
LUA_API int lua_sethook (lua_State *L, lua_Hook func, int mask, int count) {
  if (func == NULL || mask == 0) {  /* turn off hooks? */
    mask = 0;
    func = NULL;
  }
  if (isLua(L->ci))
    L->oldpc = L->ci->u.l.savedpc;
  L->hook = func;
  L->basehookcount = count;
  resethookcount(L);
  L->hookmask = cast_byte(mask);
  return 1;
}


LUA_API lua_Hook lua_gethook (lua_State *L) {
  return L->hook;
}


LUA_API int lua_gethookmask (lua_State *L) {
  return L->hookmask;
}


LUA_API int lua_gethookcount (lua_State *L) {
  return L->basehookcount;
}


LUA_API int lua_getstack (lua_State *L, int level, lua_Debug *ar) {
  int status;
  CallInfo *ci;
  if (level < 0) return 0;  /* invalid (negative) level */
  lua_lock(L);
  for (ci = L->ci; level > 0 && ci != &L->base_ci; ci = ci->previous)
    level--;
  if (level == 0 && ci != &L->base_ci) {  /* level found? */
    status = 1;
    ar->i_ci = ci;
  }
  else status = 0;  /* no such level */
  lua_unlock(L);
  return status;
}


static const char *findlocal (lua_State *L, CallInfo *ci, int n,
                              StkId *pos) {
  const char *name = NULL;
  StkId base;
  if (isLua(ci)) {
    base = ci->u.l.base;
    name = luaF_getlocalname(ci_func(ci)->l.p, n, currentpc(ci));
  }
  else
    base = ci->func + 1;
  if (name == NULL) {  /* no 'standard' name? */
    StkId limit = (ci == L->ci) ? L->top : ci->next->func;
    if (limit - base >= n && n > 0)  /* is 'n' inside 'ci' stack? */
      name = "(*temporary)";  /* generic name for any valid slot */
    else {
      *pos = base;  /* to avoid warnings */
      return NULL;  /* no name */
    }
  }
  *pos = base + (n - 1);
  return name;
}


LUA_API const char *lua_getlocal (lua_State *L, const lua_Debug *ar, int n) {
  StkId pos;
  const char *name = findlocal(L, ar->i_ci, n, &pos);
  lua_lock(L);
  if (name) {
    setobj2s(L, L->top, pos);
    api_incr_top(L);
  }
  lua_unlock(L);
  return name;
}


LUA_API const char *lua_setlocal (lua_State *L, const lua_Debug *ar, int n) {
  StkId pos;
  const char *name = findlocal(L, ar->i_ci, n, &pos);
  lua_lock(L);
  if (name)
      setobjs2s(L, pos, L->top - 1);
  L->top--;  /* pop value */
  lua_unlock(L);
  return name;
}


static void funcinfo (lua_Debug *ar, Closure *cl) {
  if (cl->c.isC) {
    ar->source = "=[C]";
    ar->linedefined = -1;
    ar->lastlinedefined = -1;
    ar->what = "C";
  }
  else {
    ar->source = getstr(cl->l.p->source);
    ar->linedefined = cl->l.p->linedefined;
    ar->lastlinedefined = cl->l.p->lastlinedefined;
    ar->what = (ar->linedefined == 0) ? "main" : "Lua";
  }
  luaO_chunkid(ar->short_src, ar->source, LUA_IDSIZE);
}


static void collectvalidlines (lua_State *L, Closure *f) {
  if (f == NULL || f->c.isC) {
    setnilvalue(L->top);
    incr_top(L);
  }
  else {
    int i;
    int *lineinfo = f->l.p->lineinfo;
    Table *t = luaH_new(L);
    sethvalue(L, L->top, t);
    incr_top(L);
    for (i=0; i<f->l.p->sizelineinfo; i++)
      setbvalue(luaH_setint(L, t, lineinfo[i]), 1);
  }
}


static int auxgetinfo (lua_State *L, const char *what, lua_Debug *ar,
                    Closure *f, CallInfo *ci) {
  int status = 1;
  for (; *what; what++) {
    switch (*what) {
      case 'S': {
        funcinfo(ar, f);
        break;
      }
      case 'l': {
        ar->currentline = (ci && isLua(ci)) ? currentline(ci) : -1;
        break;
      }
      case 'u': {
        ar->nups = f->c.nupvalues;
        if (f->c.isC) {
          ar->isvararg = 1;
          ar->nparams = 0;
        }
        else {
          ar->isvararg = f->l.p->is_vararg;
          ar->nparams = f->l.p->numparams;
        }
        break;
      }
      case 't': {
        ar->istailcall = (ci) ? ci->callstatus & CIST_TAIL : 0;
        break;
      }
      case 'n': {
        ar->namewhat = (ci) ? getfuncname(L, ci, &ar->name) : NULL;
        if (ar->namewhat == NULL) {
          ar->namewhat = "";  /* not found */
          ar->name = NULL;
        }
        break;
      }
      case 'L':
      case 'f':  /* handled by lua_getinfo */
        break;
      default: status = 0;  /* invalid option */
    }
  }
  return status;
}


LUA_API int lua_getinfo (lua_State *L, const char *what, lua_Debug *ar) {
  int status;
  Closure *f = NULL;
  CallInfo *ci = NULL;
  lua_lock(L);
  if (*what == '>') {
    StkId func = L->top - 1;
    luai_apicheck(L, ttisfunction(func));
    what++;  /* skip the '>' */
    f = clvalue(func);
    L->top--;  /* pop function */
  }
  else {
    ci = ar->i_ci;
    lua_assert(ttisfunction(ci->func));
    f = clvalue(ci->func);
  }
  status = auxgetinfo(L, what, ar, f, ci);
  if (strchr(what, 'f')) {
    setclvalue(L, L->top, f);
    incr_top(L);
  }
  if (strchr(what, 'L'))
    collectvalidlines(L, f);
  lua_unlock(L);
  return status;
}


/*
** {======================================================
** Symbolic Execution
** =======================================================
*/


static const char *kname (Proto *p, int c) {
  if (ISK(c) && ttisstring(&p->k[INDEXK(c)]))
    return svalue(&p->k[INDEXK(c)]);
  else
    return "?";
}


static const char *getobjname (lua_State *L, CallInfo *ci, int reg,
                               const char **name) {
  Proto *p;
  int lastpc, pc;
  const char *what = NULL;
  lua_assert(isLua(ci));
  p = ci_func(ci)->l.p;
  lastpc = currentpc(ci);
  *name = luaF_getlocalname(p, reg + 1, lastpc);
  if (*name)  /* is a local? */
    return "local";
  /* else try symbolic execution */
  for (pc = 0; pc < lastpc; pc++) {
    Instruction i = p->code[pc];
    OpCode op = GET_OPCODE(i);
    int a = GETARG_A(i);
    switch (op) {
      case OP_GETGLOBAL: {
        if (reg == a) {
          int g = GETARG_Bx(i);
          if (g != 0) g--;
          else g = GETARG_Ax(p->code[++pc]);
          lua_assert(ttisstring(&p->k[g]));
          *name = svalue(&p->k[g]);
          what = "global";
        }
        break;
      }
      case OP_MOVE: {
        if (reg == a) {
          int b = GETARG_B(i);  /* move from 'b' to 'a' */
          if (b < a)
            what = getobjname(L, ci, b, name);  /* get name for 'b' */
          else what = NULL;
        }
        break;
      }
      case OP_GETTABLE: {
        if (reg == a) {
          int k = GETARG_C(i);  /* key index */
          *name = kname(p, k);
          what = "field";
        }
        break;
      }
      case OP_GETUPVAL: {
        if (reg == a) {
          int u = GETARG_B(i);  /* upvalue index */
          TString *tn = p->upvalues[u].name;
          *name = tn ? getstr(tn) : "?";
          what = "upvalue";
        }
        break;
      }
      case OP_LOADNIL: {
        int b = GETARG_B(i);  /* move from 'b' to 'a' */
        if (a <= reg && reg <= b)  /* set registers from 'a' to 'b' */
          what = NULL;
        break;
      }
      case OP_SELF: {
        if (reg == a) {
          int k = GETARG_C(i);  /* key index */
          *name = kname(p, k);
          what = "method";
        }
        break;
      }
      case OP_TFORCALL: {
        if (reg >= a + 2) what = NULL;  /* affect all regs above its base */
        break;
      }
      case OP_CALL:
      case OP_TAILCALL: {
        if (reg >= a) what = NULL;  /* affect all registers above base */
        break;
      }
      case OP_JMP: {
        int b = GETARG_sBx(i);
        int dest = pc + 1 + b;
        /* jump is forward and do not skip `lastpc'? */
        if (pc < dest && dest <= lastpc)
          pc += b;  /* do the jump */
        break;
      }
      default:
        if (testAMode(op) && reg == a) what = NULL;
        break;
    }
  }
  return what;
}


static const char *getfuncname (lua_State *L, CallInfo *ci, const char **name) {
  TMS tm;
  Instruction i;
  if ((ci->callstatus & CIST_TAIL) || !isLua(ci->previous))
    return NULL;  /* calling function is not Lua (or is unknown) */
  ci = ci->previous;  /* calling function */
  i = ci_func(ci)->l.p->code[currentpc(ci)];
  if (GET_OPCODE(i) == OP_EXTRAARG)  /* extra argument? */
    i = ci_func(ci)->l.p->code[currentpc(ci) - 1];  /* get 'real' instruction */
  switch (GET_OPCODE(i)) {
    case OP_CALL:
    case OP_TAILCALL:
    case OP_TFORLOOP:
      return getobjname(L, ci, GETARG_A(i), name);
    case OP_GETGLOBAL:
    case OP_SELF:
    case OP_GETTABLE: tm = TM_INDEX; break;
    case OP_SETGLOBAL:
    case OP_SETTABLE: tm = TM_NEWINDEX; break;
    case OP_EQ: tm = TM_EQ; break;
    case OP_ADD: tm = TM_ADD; break;
    case OP_SUB: tm = TM_SUB; break;
    case OP_MUL: tm = TM_MUL; break;
    case OP_DIV: tm = TM_DIV; break;
    case OP_MOD: tm = TM_MOD; break;
    case OP_POW: tm = TM_POW; break;
    case OP_UNM: tm = TM_UNM; break;
    case OP_LEN: tm = TM_LEN; break;
    case OP_LT: tm = TM_LT; break;
    case OP_LE: tm = TM_LE; break;
    case OP_CONCAT: tm = TM_CONCAT; break;
    default:
      return NULL;  /* else no useful name can be found */
  }
  *name = getstr(G(L)->tmname[tm]);
  return "metamethod";
}

/* }====================================================== */



/* only ANSI way to check whether a pointer points to an array */
static int isinstack (CallInfo *ci, const TValue *o) {
  StkId p;
  for (p = ci->u.l.base; p < ci->top; p++)
    if (o == p) return 1;
  return 0;
}


void luaG_typeerror (lua_State *L, const TValue *o, const char *op) {
  CallInfo *ci = L->ci;
  const char *name = NULL;
  const char *t = typename(ttype(o));
  const char *kind = (isLua(ci) && isinstack(ci, o)) ?
                         getobjname(L, ci, cast_int(o - ci->u.l.base), &name) :
                         NULL;
  if (kind)
    luaG_runerror(L, "attempt to %s %s " LUA_QS " (a %s value)",
                op, kind, name, t);
  else
    luaG_runerror(L, "attempt to %s a %s value", op, t);
}


void luaG_concaterror (lua_State *L, StkId p1, StkId p2) {
  if (ttisstring(p1) || ttisnumber(p1)) p1 = p2;
  lua_assert(!ttisstring(p1) && !ttisnumber(p2));
  luaG_typeerror(L, p1, "concatenate");
}


void luaG_aritherror (lua_State *L, const TValue *p1, const TValue *p2) {
  TValue temp;
  if (luaV_tonumber(p1, &temp) == NULL)
    p2 = p1;  /* first operand is wrong */
  luaG_typeerror(L, p2, "perform arithmetic on");
}


int luaG_ordererror (lua_State *L, const TValue *p1, const TValue *p2) {
  const char *t1 = typename(ttype(p1));
  const char *t2 = typename(ttype(p2));
  if (t1 == t2)
    luaG_runerror(L, "attempt to compare two %s values", t1);
  else
    luaG_runerror(L, "attempt to compare %s with %s", t1, t2);
  return 0;
}


static void addinfo (lua_State *L, const char *msg) {
  CallInfo *ci = L->ci;
  if (isLua(ci)) {  /* is Lua code? */
    char buff[LUA_IDSIZE];  /* add file:line information */
    int line = currentline(ci);
    luaO_chunkid(buff, getstr(ci_func(ci)->l.p->source), LUA_IDSIZE);
    luaO_pushfstring(L, "%s:%d: %s", buff, line, msg);
  }
}


void luaG_errormsg (lua_State *L) {
  if (L->errfunc != 0) {  /* is there an error handling function? */
    StkId errfunc = restorestack(L, L->errfunc);
    if (!ttisfunction(errfunc)) luaD_throw(L, LUA_ERRERR);
    setobjs2s(L, L->top, L->top - 1);  /* move argument */
    setobjs2s(L, L->top - 1, errfunc);  /* push function */
    incr_top(L);
    luaD_call(L, L->top - 2, 1, 0);  /* call it */
  }
  luaD_throw(L, LUA_ERRRUN);
}


void luaG_runerror (lua_State *L, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  addinfo(L, luaO_pushvfstring(L, fmt, argp));
  va_end(argp);
  luaG_errormsg(L);
}

