/*
** $Id: ldebug.c,v 1.150 2003/03/19 21:24:04 roberto Exp $
** Debug Interface
** See Copyright Notice in lua.h
*/


#include <stdlib.h>
#include <string.h>

#define ldebug_c

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



static const char *getfuncname (CallInfo *ci, const char **name);


#define isLua(ci)	(!((ci)->state & CI_C))


static int currentpc (CallInfo *ci) {
  if (!isLua(ci)) return -1;  /* function is not a Lua function? */
  if (ci->state & CI_HASFRAME)  /* function has a frame? */
    ci->u.l.savedpc = *ci->u.l.pc;  /* use `pc' from there */
  /* function's pc is saved */
  return pcRel(ci->u.l.savedpc, ci_func(ci)->l.p);
}


static int currentline (CallInfo *ci) {
  int pc = currentpc(ci);
  if (pc < 0)
    return -1;  /* only active lua functions have current-line information */
  else
    return getline(ci_func(ci)->l.p, pc);
}


void luaG_inithooks (lua_State *L) {
  CallInfo *ci;
  for (ci = L->ci; ci != L->base_ci; ci--)  /* update all `savedpc's */
    currentpc(ci);
  L->hookinit = 1;
}


/*
** this function can be called asynchronous (e.g. during a signal)
*/
LUA_API int lua_sethook (lua_State *L, lua_Hook func, int mask, int count) {
  if (func == NULL || mask == 0) {  /* turn off hooks? */
    mask = 0;
    func = NULL;
  }
  L->hook = func;
  L->basehookcount = count;
  resethookcount(L);
  L->hookmask = cast(lu_byte, mask);
  L->hookinit = 0;
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
  lua_lock(L);
  for (ci = L->ci; level > 0 && ci > L->base_ci; ci--) {
    level--;
    if (!(ci->state & CI_C))  /* Lua function? */
      level -= ci->u.l.tailcalls;  /* skip lost tail calls */
  }
  if (level > 0 || ci == L->base_ci) status = 0;  /* there is no such level */
  else if (level < 0) {  /* level is of a lost tail call */
    status = 1;
    ar->i_ci = 0;
  }
  else {
    status = 1;
    ar->i_ci = ci - L->base_ci;
  }
  lua_unlock(L);
  return status;
}


static Proto *getluaproto (CallInfo *ci) {
  return (isLua(ci) ? ci_func(ci)->l.p : NULL);
}


LUA_API const char *lua_getlocal (lua_State *L, const lua_Debug *ar, int n) {
  const char *name;
  CallInfo *ci;
  Proto *fp;
  lua_lock(L);
  name = NULL;
  ci = L->base_ci + ar->i_ci;
  fp = getluaproto(ci);
  if (fp) {  /* is a Lua function? */
    name = luaF_getlocalname(fp, n, currentpc(ci));
    if (name)
      luaA_pushobject(L, ci->base+(n-1));  /* push value */
  }
  lua_unlock(L);
  return name;
}


LUA_API const char *lua_setlocal (lua_State *L, const lua_Debug *ar, int n) {
  const char *name;
  CallInfo *ci;
  Proto *fp;
  lua_lock(L);
  name = NULL;
  ci = L->base_ci + ar->i_ci;
  fp = getluaproto(ci);
  L->top--;  /* pop new value */
  if (fp) {  /* is a Lua function? */
    name = luaF_getlocalname(fp, n, currentpc(ci));
    if (!name || name[0] == '(')  /* `(' starts private locals */
      name = NULL;
    else
      setobjs2s(ci->base+(n-1), L->top);
  }
  lua_unlock(L);
  return name;
}


static void funcinfo (lua_Debug *ar, StkId func) {
  Closure *cl = clvalue(func);
  if (cl->c.isC) {
    ar->source = "=[C]";
    ar->linedefined = -1;
    ar->what = "C";
  }
  else {
    ar->source = getstr(cl->l.p->source);
    ar->linedefined = cl->l.p->lineDefined;
    ar->what = (ar->linedefined == 0) ? "main" : "Lua";
  }
  luaO_chunkid(ar->short_src, ar->source, LUA_IDSIZE);
}


static const char *travglobals (lua_State *L, const TObject *o) {
  Table *g = hvalue(gt(L));
  int i = sizenode(g);
  while (i--) {
    Node *n = gnode(g, i);
    if (luaO_rawequalObj(o, gval(n)) && ttisstring(gkey(n)))
      return getstr(tsvalue(gkey(n)));
  }
  return NULL;
}


static void info_tailcall (lua_State *L, lua_Debug *ar) {
  ar->name = ar->namewhat = "";
  ar->what = "tail";
  ar->linedefined = ar->currentline = -1;
  ar->source = "=(tail call)";
  luaO_chunkid(ar->short_src, ar->source, LUA_IDSIZE);
  ar->nups = 0;
  setnilvalue(L->top);
}


static int auxgetinfo (lua_State *L, const char *what, lua_Debug *ar,
                    StkId f, CallInfo *ci) {
  int status = 1;
  for (; *what; what++) {
    switch (*what) {
      case 'S': {
        funcinfo(ar, f);
        break;
      }
      case 'l': {
        ar->currentline = (ci) ? currentline(ci) : -1;
        break;
      }
      case 'u': {
        ar->nups = clvalue(f)->c.nupvalues;
        break;
      }
      case 'n': {
        ar->namewhat = (ci) ? getfuncname(ci, &ar->name) : NULL;
        if (ar->namewhat == NULL) {
          /* try to find a global name */
          if ((ar->name = travglobals(L, f)) != NULL)
            ar->namewhat = "global";
          else ar->namewhat = "";  /* not found */
        }
        break;
      }
      case 'f': {
        setobj2s(L->top, f);
        break;
      }
      default: status = 0;  /* invalid option */
    }
  }
  return status;
}


LUA_API int lua_getinfo (lua_State *L, const char *what, lua_Debug *ar) {
  int status = 1;
  lua_lock(L);
  if (*what == '>') {
    StkId f = L->top - 1;
    if (!ttisfunction(f))
      luaG_runerror(L, "value for `lua_getinfo' is not a function");
    status = auxgetinfo(L, what + 1, ar, f, NULL);
    L->top--;  /* pop function */
  }
  else if (ar->i_ci != 0) {  /* no tail call? */
    CallInfo *ci = L->base_ci + ar->i_ci;
    lua_assert(ttisfunction(ci->base - 1));
    status = auxgetinfo(L, what, ar, ci->base - 1, ci);
  }
  else
    info_tailcall(L, ar);
  if (strchr(what, 'f')) incr_top(L);
  lua_unlock(L);
  return status;
}


/*
** {======================================================
** Symbolic Execution and code checker
** =======================================================
*/

#define check(x)		if (!(x)) return 0;

#define checkjump(pt,pc)	check(0 <= pc && pc < pt->sizecode)

#define checkreg(pt,reg)	check((reg) < (pt)->maxstacksize)



static int precheck (const Proto *pt) {
  check(pt->maxstacksize <= MAXSTACK);
  check(pt->sizelineinfo == pt->sizecode || pt->sizelineinfo == 0);
  lua_assert(pt->numparams+pt->is_vararg <= pt->maxstacksize);
  check(GET_OPCODE(pt->code[pt->sizecode-1]) == OP_RETURN);
  return 1;
}


static int checkopenop (const Proto *pt, int pc) {
  Instruction i = pt->code[pc+1];
  switch (GET_OPCODE(i)) {
    case OP_CALL:
    case OP_TAILCALL:
    case OP_RETURN: {
      check(GETARG_B(i) == 0);
      return 1;
    }
    case OP_SETLISTO: return 1;
    default: return 0;  /* invalid instruction after an open call */
  }
}


static int checkRK (const Proto *pt, int r) {
  return (r < pt->maxstacksize || (r >= MAXSTACK && r-MAXSTACK < pt->sizek));
}


static Instruction luaG_symbexec (const Proto *pt, int lastpc, int reg) {
  int pc;
  int last;  /* stores position of last instruction that changed `reg' */
  last = pt->sizecode-1;  /* points to final return (a `neutral' instruction) */
  check(precheck(pt));
  for (pc = 0; pc < lastpc; pc++) {
    const Instruction i = pt->code[pc];
    OpCode op = GET_OPCODE(i);
    int a = GETARG_A(i);
    int b = 0;
    int c = 0;
    checkreg(pt, a);
    switch (getOpMode(op)) {
      case iABC: {
        b = GETARG_B(i);
        c = GETARG_C(i);
        if (testOpMode(op, OpModeBreg)) {
          checkreg(pt, b);
        }
        else if (testOpMode(op, OpModeBrk))
          check(checkRK(pt, b));
        if (testOpMode(op, OpModeCrk))
          check(checkRK(pt, c));
        break;
      }
      case iABx: {
        b = GETARG_Bx(i);
        if (testOpMode(op, OpModeK)) check(b < pt->sizek);
        break;
      }
      case iAsBx: {
        b = GETARG_sBx(i);
        break;
      }
    }
    if (testOpMode(op, OpModesetA)) {
      if (a == reg) last = pc;  /* change register `a' */
    }
    if (testOpMode(op, OpModeT)) {
      check(pc+2 < pt->sizecode);  /* check skip */
      check(GET_OPCODE(pt->code[pc+1]) == OP_JMP);
    }
    switch (op) {
      case OP_LOADBOOL: {
        check(c == 0 || pc+2 < pt->sizecode);  /* check its jump */
        break;
      }
      case OP_LOADNIL: {
        if (a <= reg && reg <= b)
          last = pc;  /* set registers from `a' to `b' */
        break;
      }
      case OP_GETUPVAL:
      case OP_SETUPVAL: {
        check(b < pt->nups);
        break;
      }
      case OP_GETGLOBAL:
      case OP_SETGLOBAL: {
        check(ttisstring(&pt->k[b]));
        break;
      }
      case OP_SELF: {
        checkreg(pt, a+1);
        if (reg == a+1) last = pc;
        break;
      }
      case OP_CONCAT: {
        /* `c' is a register, and at least two operands */
        check(c < MAXSTACK && b < c);
        break;
      }
      case OP_TFORLOOP:
        checkreg(pt, a+c+5);
        if (reg >= a) last = pc;  /* affect all registers above base */
        /* go through */
      case OP_FORLOOP:
        checkreg(pt, a+2);
        /* go through */
      case OP_JMP: {
        int dest = pc+1+b;
	check(0 <= dest && dest < pt->sizecode);
        /* not full check and jump is forward and do not skip `lastpc'? */
        if (reg != NO_REG && pc < dest && dest <= lastpc)
          pc += b;  /* do the jump */
        break;
      }
      case OP_CALL:
      case OP_TAILCALL: {
        if (b != 0) {
          checkreg(pt, a+b-1);
        }
        c--;  /* c = num. returns */
        if (c == LUA_MULTRET) {
          check(checkopenop(pt, pc));
        }
        else if (c != 0)
          checkreg(pt, a+c-1);
        if (reg >= a) last = pc;  /* affect all registers above base */
        break;
      }
      case OP_RETURN: {
        b--;  /* b = num. returns */
        if (b > 0) checkreg(pt, a+b-1);
        break;
      }
      case OP_SETLIST: {
        checkreg(pt, a + (b&(LFIELDS_PER_FLUSH-1)) + 1);
        break;
      }
      case OP_CLOSURE: {
        int nup;
        check(b < pt->sizep);
        nup = pt->p[b]->nups;
        check(pc + nup < pt->sizecode);
        for (; nup>0; nup--) {
          OpCode op1 = GET_OPCODE(pt->code[pc+nup]);
          check(op1 == OP_GETUPVAL || op1 == OP_MOVE);
        }
        break;
      }
      default: break;
    }
  }
  return pt->code[last];
}

#undef check
#undef checkjump
#undef checkreg

/* }====================================================== */


int luaG_checkcode (const Proto *pt) {
  return luaG_symbexec(pt, pt->sizecode, NO_REG);
}


static const char *kname (Proto *p, int c) {
  c = c - MAXSTACK;
  if (c >= 0 && ttisstring(&p->k[c]))
    return svalue(&p->k[c]);
  else
    return "?";
}


static const char *getobjname (CallInfo *ci, int stackpos, const char **name) {
  if (isLua(ci)) {  /* a Lua function? */
    Proto *p = ci_func(ci)->l.p;
    int pc = currentpc(ci);
    Instruction i;
    *name = luaF_getlocalname(p, stackpos+1, pc);
    if (*name)  /* is a local? */
      return "local";
    i = luaG_symbexec(p, pc, stackpos);  /* try symbolic execution */
    lua_assert(pc != -1);
    switch (GET_OPCODE(i)) {
      case OP_GETGLOBAL: {
        int g = GETARG_Bx(i);  /* global index */
        lua_assert(ttisstring(&p->k[g]));
        *name = svalue(&p->k[g]);
        return "global";
      }
      case OP_MOVE: {
        int a = GETARG_A(i);
        int b = GETARG_B(i);  /* move from `b' to `a' */
        if (b < a)
          return getobjname(ci, b, name);  /* get name for `b' */
        break;
      }
      case OP_GETTABLE: {
        int k = GETARG_C(i);  /* key index */
        *name = kname(p, k);
        return "field";
      }
      case OP_SELF: {
        int k = GETARG_C(i);  /* key index */
        *name = kname(p, k);
        return "method";
      }
      default: break;
    }
  }
  return NULL;  /* no useful name found */
}


static const char *getfuncname (CallInfo *ci, const char **name) {
  Instruction i;
  if ((isLua(ci) && ci->u.l.tailcalls > 0) || !isLua(ci - 1))
    return NULL;  /* calling function is not Lua (or is unknown) */
  ci--;  /* calling function */
  i = ci_func(ci)->l.p->code[currentpc(ci)];
  if (GET_OPCODE(i) == OP_CALL || GET_OPCODE(i) == OP_TAILCALL)
    return getobjname(ci, GETARG_A(i), name);
  else
    return NULL;  /* no useful name can be found */
}


/* only ANSI way to check whether a pointer points to an array */
static int isinstack (CallInfo *ci, const TObject *o) {
  StkId p;
  for (p = ci->base; p < ci->top; p++)
    if (o == p) return 1;
  return 0;
}


void luaG_typeerror (lua_State *L, const TObject *o, const char *op) {
  const char *name = NULL;
  const char *t = luaT_typenames[ttype(o)];
  const char *kind = (isinstack(L->ci, o)) ?
                         getobjname(L->ci, o - L->base, &name) : NULL;
  if (kind)
    luaG_runerror(L, "attempt to %s %s `%s' (a %s value)",
                op, kind, name, t);
  else
    luaG_runerror(L, "attempt to %s a %s value", op, t);
}


void luaG_concaterror (lua_State *L, StkId p1, StkId p2) {
  if (ttisstring(p1)) p1 = p2;
  lua_assert(!ttisstring(p1));
  luaG_typeerror(L, p1, "concatenate");
}


void luaG_aritherror (lua_State *L, const TObject *p1, const TObject *p2) {
  TObject temp;
  if (luaV_tonumber(p1, &temp) == NULL)
    p2 = p1;  /* first operand is wrong */
  luaG_typeerror(L, p2, "perform arithmetic on");
}


int luaG_ordererror (lua_State *L, const TObject *p1, const TObject *p2) {
  const char *t1 = luaT_typenames[ttype(p1)];
  const char *t2 = luaT_typenames[ttype(p2)];
  if (t1[2] == t2[2])
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
    luaO_chunkid(buff, getstr(getluaproto(ci)->source), LUA_IDSIZE);
    luaO_pushfstring(L, "%s:%d: %s", buff, line, msg);
  }
}


void luaG_errormsg (lua_State *L) {
  if (L->errfunc != 0) {  /* is there an error handling function? */
    StkId errfunc = restorestack(L, L->errfunc);
    if (!ttisfunction(errfunc)) luaD_throw(L, LUA_ERRERR);
    setobjs2s(L->top, L->top - 1);  /* move argument */
    setobjs2s(L->top - 1, errfunc);  /* push function */
    incr_top(L);
    luaD_call(L, L->top - 2, 1);  /* call it */
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

