/*
** $Id: ldebug.c,v 2.82 2011/06/02 19:31:40 roberto Exp $
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
  return pcRel(ci->u.l.savedpc, ci_func(ci)->p);
}


static int currentline (CallInfo *ci) {
  return getfuncline(ci_func(ci)->p, currentpc(ci));
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


static const char *findvararg (CallInfo *ci, int n, StkId *pos) {
  int nparams = clLvalue(ci->func)->p->numparams;
  if (n >= ci->u.l.base - ci->func - nparams)
    return NULL;  /* no such vararg */
  else {
    *pos = ci->func + nparams + n;
    return "(*vararg)";  /* generic name for any vararg */
  }
}


static const char *findlocal (lua_State *L, CallInfo *ci, int n,
                              StkId *pos) {
  const char *name = NULL;
  StkId base;
  if (isLua(ci)) {
    if (n < 0)  /* access to vararg values? */
      return findvararg(ci, -n, pos);
    else {
      base = ci->u.l.base;
      name = luaF_getlocalname(ci_func(ci)->p, n, currentpc(ci));
    }
  }
  else
    base = ci->func + 1;
  if (name == NULL) {  /* no 'standard' name? */
    StkId limit = (ci == L->ci) ? L->top : ci->next->func;
    if (limit - base >= n && n > 0)  /* is 'n' inside 'ci' stack? */
      name = "(*temporary)";  /* generic name for any valid slot */
    else
      return NULL;  /* no name */
  }
  *pos = base + (n - 1);
  return name;
}


LUA_API const char *lua_getlocal (lua_State *L, const lua_Debug *ar, int n) {
  const char *name;
  lua_lock(L);
  if (ar == NULL) {  /* information about non-active function? */
    if (!isLfunction(L->top - 1))  /* not a Lua function? */
      name = NULL;
    else  /* consider live variables at function start (parameters) */
      name = luaF_getlocalname(clLvalue(L->top - 1)->p, n, 0);
  }
  else {  /* active function; get information through 'ar' */
    StkId pos = 0;  /* to avoid warnings */
    name = findlocal(L, ar->i_ci, n, &pos);
    if (name) {
      setobj2s(L, L->top, pos);
      api_incr_top(L);
    }
  }
  lua_unlock(L);
  return name;
}


LUA_API const char *lua_setlocal (lua_State *L, const lua_Debug *ar, int n) {
  StkId pos = 0;  /* to avoid warnings */
  const char *name = findlocal(L, ar->i_ci, n, &pos);
  lua_lock(L);
  if (name)
    setobjs2s(L, pos, L->top - 1);
  L->top--;  /* pop value */
  lua_unlock(L);
  return name;
}


static void funcinfo (lua_Debug *ar, Closure *cl) {
  if (cl == NULL || cl->c.isC) {
    ar->source = "=[C]";
    ar->linedefined = -1;
    ar->lastlinedefined = -1;
    ar->what = "C";
  }
  else {
    Proto *p = cl->l.p;
    ar->source = p->source ? getstr(p->source) : "=?";
    ar->linedefined = p->linedefined;
    ar->lastlinedefined = p->lastlinedefined;
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
        ar->nups = (f == NULL) ? 0 : f->c.nupvalues;
        if (f == NULL || f->c.isC) {
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
  Closure *cl;
  CallInfo *ci;
  StkId func;
  lua_lock(L);
  if (*what == '>') {
    ci = NULL;
    func = L->top - 1;
    api_check(L, ttisfunction(func), "function expected");
    what++;  /* skip the '>' */
    L->top--;  /* pop function */
  }
  else {
    ci = ar->i_ci;
    func = ci->func;
    lua_assert(ttisfunction(ci->func));
  }
  cl = ttisclosure(func) ? clvalue(func) : NULL;
  status = auxgetinfo(L, what, ar, cl, ci);
  if (strchr(what, 'f')) {
    setobjs2s(L, L->top, func);
    incr_top(L);
  }
  if (strchr(what, 'L'))
    collectvalidlines(L, cl);
  lua_unlock(L);
  return status;
}


/*
** {======================================================
** Symbolic Execution
** =======================================================
*/

static const char *getobjname (lua_State *L, CallInfo *ci, int reg,
                               const char **name);


/*
** find a "name" for the RK value 'c'
*/
static void kname (lua_State *L, CallInfo *ci, int c, int oreg,
                   const char *what, const char **name) {
  if (ISK(c)) {  /* is 'c' a constant? */
    TValue *kvalue = &ci_func(ci)->p->k[INDEXK(c)];
    if (ttisstring(kvalue)) {  /* literal constant? */
      *name = svalue(kvalue);  /* it is its own name */
      return;
    }
    /* else no reasonable name found */
  }
  else {  /* 'c' is a register */
    if (c != oreg)  /* not the original register? */
      what = getobjname(L, ci, c, name); /* search for 'c' */
    if (what && *what == 'c') {  /* found a constant name? */
      return;  /* 'name' already filled */
    }
    /* else no reasonable name found */
  }
  *name = "?";  /* no reasonable name found */
}


static const char *getobjname (lua_State *L, CallInfo *ci, int reg,
                               const char **name) {
  Proto *p = ci_func(ci)->p;
  const char *what = NULL;
  int lastpc = currentpc(ci);
  int pc;
  *name = luaF_getlocalname(p, reg + 1, lastpc);
  if (*name)  /* is a local? */
    return "local";
  /* else try symbolic execution */
  for (pc = 0; pc < lastpc; pc++) {
    Instruction i = p->code[pc];
    OpCode op = GET_OPCODE(i);
    int a = GETARG_A(i);
    switch (op) {
      case OP_MOVE: {
        if (reg == a) {
          int b = GETARG_B(i);  /* move from 'b' to 'a' */
          if (b < a)
            what = getobjname(L, ci, b, name);  /* get name for 'b' */
          else what = NULL;
        }
        break;
      }
      case OP_GETTABUP:
      case OP_GETTABLE: {
        if (reg == a) {
          int k = GETARG_C(i);  /* key index */
          int t = GETARG_B(i);
          const char *vn = (op == OP_GETTABLE)  /* name of indexed variable */
                           ? luaF_getlocalname(p, t + 1, pc)
                           : getstr(p->upvalues[t].name);
          kname(L, ci, k, a, what, name);
          what = (vn && strcmp(vn, LUA_ENV) == 0) ? "global" : "field";
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
      case OP_LOADK:
      case OP_LOADKX: {
        if (reg == a) {
          int b = (op == OP_LOADK) ? GETARG_Bx(i)
                                   : GETARG_Ax(p->code[pc + 1]);
          if (ttisstring(&p->k[b])) {
            what = "constant";
            *name = svalue(&p->k[b]);
          }
        }
        break;
      }
      case OP_LOADNIL: {
        int b = GETARG_B(i);
        if (a <= reg && reg <= a + b)  /* set registers from 'a' to 'a+b' */
          what = NULL;
        break;
      }
      case OP_SELF: {
        if (reg == a) {
          int k = GETARG_C(i);  /* key index */
          kname(L, ci, k, a, what, name);
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
      case OP_TEST: {
        if (reg == a) what = NULL;  /* jumped code can change 'a' */
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
  i = ci_func(ci)->p->code[currentpc(ci)];
  if (GET_OPCODE(i) == OP_EXTRAARG)  /* extra argument? */
    i = ci_func(ci)->p->code[currentpc(ci) - 1];  /* get 'real' instruction */
  switch (GET_OPCODE(i)) {
    case OP_CALL:
    case OP_TAILCALL:
      return getobjname(L, ci, GETARG_A(i), name);
    case OP_TFORCALL: {
      *name = "for iterator";
       return "for iterator";
    }
    case OP_SELF:
    case OP_GETTABUP:
    case OP_GETTABLE: tm = TM_INDEX; break;
    case OP_SETTABUP:
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



/*
** only ANSI way to check whether a pointer points to an array
** (used only for error messages, so efficiency is not a big concern)
*/
static int isinstack (CallInfo *ci, const TValue *o) {
  StkId p;
  for (p = ci->u.l.base; p < ci->top; p++)
    if (o == p) return 1;
  return 0;
}


static const char *getupvalname (CallInfo *ci, const TValue *o,
                               const char **name) {
  LClosure *c = ci_func(ci);
  int i;
  for (i = 0; i < c->nupvalues; i++) {
    if (c->upvals[i]->v == o) {
      *name = getstr(c->p->upvalues[i].name);
      return "upvalue";
    }
  }
  return NULL;
}


void luaG_typeerror (lua_State *L, const TValue *o, const char *op) {
  CallInfo *ci = L->ci;
  const char *name = NULL;
  const char *t = objtypename(o);
  const char *kind = NULL;
  if (isLua(ci)) {
    kind = getupvalname(ci, o, &name);  /* check whether 'o' is an upvalue */
    if (!kind && isinstack(ci, o))  /* no? try a register */
      kind = getobjname(L, ci, cast_int(o - ci->u.l.base), &name);
  }
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
  const char *t1 = objtypename(p1);
  const char *t2 = objtypename(p2);
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
    TString *src = ci_func(ci)->p->source;
    if (src)
      luaO_chunkid(buff, getstr(src), LUA_IDSIZE);
    else {  /* no source available; use "?" instead */
      buff[0] = '?'; buff[1] = '\0';
    }
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

