/*
** $Id: ldebug.c,v 1.1 2001/11/29 22:14:34 rieru Exp rieru $
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
#include "lvm.h"



static const char *getfuncname (lua_State *L, CallInfo *ci,
                                  const char **name);



static int isLmark (CallInfo *ci) {
  lua_assert(ci == NULL || ttype(ci->base - 1) == LUA_TFUNCTION);
  return (ci && ci->prev && !ci_func(ci)->c.isC);
}


LUA_API lua_Hook lua_setcallhook (lua_State *L, lua_Hook func) {
  lua_Hook oldhook;
  lua_lock(L);
  oldhook = L->callhook;
  L->callhook = func;
  lua_unlock(L);
  return oldhook;
}


LUA_API lua_Hook lua_setlinehook (lua_State *L, lua_Hook func) {
  lua_Hook oldhook;
  lua_lock(L);
  oldhook = L->linehook;
  L->linehook = func;
  lua_unlock(L);
  return oldhook;
}


static CallInfo *ci_stack (lua_State *L, StkId obj) {
  CallInfo *ci = L->ci;
  while (ci->base > obj) ci = ci->prev;
  return (ci != &L->basefunc) ? ci : NULL;
}


LUA_API int lua_getstack (lua_State *L, int level, lua_Debug *ar) {
  CallInfo *ci;
  int status;
  lua_lock(L);
  ci = L->ci;
  while (level-- && ci != &L->basefunc) {
    lua_assert(ci->base > ci->prev->base);
    ci = ci->prev;
  }
  if (ci == &L->basefunc) status = 0;  /* there is no such level */
  else {
    ar->_ci = ci;
    status = 1;
  }
  lua_unlock(L);
  return status;
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


static int currentpc (CallInfo *ci) {
  lua_assert(isLmark(ci));
  if (ci->pc)
    return (*ci->pc - ci_func(ci)->l.p->code) - 1;
  else
    return -1;  /* function is not active */
}


static int currentline (CallInfo *ci) {
  if (!isLmark(ci))
    return -1;  /* only active lua functions have current-line information */
  else {
    int *lineinfo = ci_func(ci)->l.p->lineinfo;
    return luaG_getline(lineinfo, currentpc(ci), 1, NULL);
  }
}



static Proto *getluaproto (CallInfo *ci) {
  return (isLmark(ci) ? ci_func(ci)->l.p : NULL);
}


LUA_API const char *lua_getlocal (lua_State *L, const lua_Debug *ar, int n) {
  const char *name;
  CallInfo *ci;
  Proto *fp;
  lua_lock(L);
  name = NULL;
  ci = ar->_ci;
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
  ci = ar->_ci;
  fp = getluaproto(ci);
  L->top--;  /* pop new value */
  if (fp) {  /* is a Lua function? */
    name = luaF_getlocalname(fp, n, currentpc(ci));
    if (!name || name[0] == '(')  /* `(' starts private locals */
      name = NULL;
    else
      setobj(ci->base+(n-1), L->top);
  }
  lua_unlock(L);
  return name;
}


static void infoLproto (lua_Debug *ar, Proto *f) {
  ar->source = getstr(f->source);
  ar->linedefined = f->lineDefined;
  ar->what = "Lua";
}


static void funcinfo (lua_State *L, lua_Debug *ar, StkId func) {
  Closure *cl;
  if (ttype(func) == LUA_TFUNCTION)
    cl = clvalue(func);
  else {
    luaD_error(L, "value for `lua_getinfo' is not a function");
    cl = NULL;  /* to avoid warnings */
  }
  if (cl->c.isC) {
    ar->source = "=C";
    ar->linedefined = -1;
    ar->what = "C";
  }
  else
    infoLproto(ar, cl->l.p);
  luaO_chunkid(ar->short_src, ar->source, LUA_IDSIZE);
  if (ar->linedefined == 0)
    ar->what = "main";
}


static const char *travglobals (lua_State *L, const TObject *o) {
  Table *g = hvalue(gt(L));
  int i = sizenode(g);
  while (i--) {
    Node *n = node(g, i);
    if (luaO_equalObj(o, val(n)) && ttype(key(n)) == LUA_TSTRING)
      return getstr(tsvalue(key(n)));
  }
  return NULL;
}


static void getname (lua_State *L, const TObject *f, lua_Debug *ar) {
  /* try to find a name for given function */
  if ((ar->name = travglobals(L, f)) != NULL)
    ar->namewhat = "global";
  else ar->namewhat = "";  /* not found */
}


LUA_API int lua_getinfo (lua_State *L, const char *what, lua_Debug *ar) {
  StkId f;
  CallInfo *ci;
  int status = 1;
  lua_lock(L);
  if (*what != '>') {  /* function is active? */
    ci = ar->_ci;
    f = ci->base - 1;
  }
  else {
    what++;  /* skip the `>' */
    ci = NULL;
    f = L->top - 1;
  }
  for (; *what; what++) {
    switch (*what) {
      case 'S': {
        funcinfo(L, ar, f);
        break;
      }
      case 'l': {
        ar->currentline = currentline(ci);
        break;
      }
      case 'u': {
        ar->nups = (ttype(f) == LUA_TFUNCTION) ? clvalue(f)->c.nupvalues : 0;
        break;
      }
      case 'n': {
        ar->namewhat = (ci) ? getfuncname(L, ci, &ar->name) : NULL;
        if (ar->namewhat == NULL)
          getname(L, f, ar);
        break;
      }
      case 'f': {
        setobj(L->top, f);
        incr_top;  /* push function */
        break;
      }
      default: status = 0;  /* invalid option */
    }
  }
  if (!ci) L->top--;  /* pop function */
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


static int checklineinfo (const Proto *pt) {
  int *lineinfo = pt->lineinfo;
  if (lineinfo == NULL) return 1;
  check(pt->sizelineinfo >= 2 && lineinfo[pt->sizelineinfo-1] == MAX_INT);
  if (*lineinfo < 0) lineinfo++;
  check(*lineinfo == 0);
  return 1;
}


static int precheck (const Proto *pt) {
  check(checklineinfo(pt));
  check(pt->maxstacksize <= MAXSTACK);
  check(pt->numparams+pt->is_vararg <= pt->maxstacksize);
  check(GET_OPCODE(pt->code[pt->sizecode-1]) == OP_RETURN);
  return 1;
}


static int checkopenop (const Proto *pt, int pc) {
  Instruction i = pt->code[pc+1];
  switch (GET_OPCODE(i)) {
    case OP_CALL:
    case OP_RETURN: {
      check(GETARG_B(i) == NO_REG);
      return 1;
    }
    case OP_SETLISTO: return 1;
    default: return 0;  /* invalid instruction after an open call */
  }
}


static Instruction luaG_symbexec (const Proto *pt, int lastpc, int reg) {
  int pc;
  int last;  /* stores position of last instruction that changed `reg' */
  last = pt->sizecode-1;  /* points to final return (a `neutral' instruction) */
  if (reg == NO_REG)  /* full check? */
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
        if (testOpMode(op, OpModeBreg))
          checkreg(pt, b);
        if (testOpMode(op, OpModeCreg))
          check(c < pt->maxstacksize ||
               (c >= MAXSTACK && c-MAXSTACK < pt->sizek));
        break;
      }
      case iABc: {
        b = GETARG_Bc(i);
        if (testOpMode(op, OpModeK)) check(b < pt->sizek);
        break;
      }
      case iAsBc: {
        b = GETARG_sBc(i);
        break;
      }
    }
    if (testOpMode(op, OpModesetA)) {
      if (a == reg) last = pc;  /* change register `a' */
    }
    if (testOpMode(op, OpModeT))
      check(GET_OPCODE(pt->code[pc+1]) == OP_CJMP);
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
        check(b < pt->nupvalues);
        break;
      }
      case OP_GETGLOBAL:
      case OP_SETGLOBAL: {
        check(ttype(&pt->k[b]) == LUA_TSTRING);
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
      case OP_JMP:
      case OP_CJMP: {
        int dest = pc+1+b;
	check(0 <= dest && dest < pt->sizecode);
        /* not full check and jump is forward and do not skip `lastpc'? */
        if (reg != NO_REG && pc < dest && dest <= lastpc)
          pc += b;  /* do the jump */
        break;
      }
      case OP_CALL: {
        if (b != NO_REG) {
          checkreg(pt, a+b);
        }
        if (c == NO_REG) {
          check(checkopenop(pt, pc));
        }
        else if (c != 0)
          checkreg(pt, a+c-1);
        if (reg >= a) last = pc;  /* affect all registers above base */
        break;
      }
      case OP_RETURN: {
        if (b != NO_REG && b != 0)
          checkreg(pt, a+b-1);
        break;
      }
      case OP_FORPREP:
      case OP_TFORPREP: {
        int dest = pc-b;  /* jump is negated here */
        check(0 <= dest && dest < pt->sizecode &&
              GET_OPCODE(pt->code[dest]) == op+1);
        break;
      }
      case OP_FORLOOP:
      case OP_TFORLOOP: {
        int dest = pc+b;
        check(0 <= dest && dest < pt->sizecode &&
              pt->code[dest] == SET_OPCODE(i, op-1));
        checkreg(pt, a + ((op == OP_FORLOOP) ? 2 : 3));
        break;
      }
      case OP_SETLIST: {
        checkreg(pt, a + (b&(LFIELDS_PER_FLUSH-1)) + 1);
        break;
      }
      case OP_CLOSURE: {
        check(b < pt->sizep);
        check(pc + pt->p[b]->nupvalues < pt->sizecode);
        break;
      }
      default: break;
    }
  }
  return pt->code[last];
}

/* }====================================================== */


int luaG_checkcode (const Proto *pt) {
  return luaG_symbexec(pt, pt->sizecode, NO_REG);
}


static const char *getobjname (lua_State *L, StkId obj, const char **name) {
  CallInfo *ci = ci_stack(L, obj);
  if (isLmark(ci)) {  /* an active Lua function? */
    Proto *p = ci_func(ci)->l.p;
    int pc = currentpc(ci);
    int stackpos = obj - ci->base;
    Instruction i;
    *name = luaF_getlocalname(p, stackpos+1, pc);
    if (*name)  /* is a local? */
      return "local";
    i = luaG_symbexec(p, pc, stackpos);  /* try symbolic execution */
    lua_assert(pc != -1);
    switch (GET_OPCODE(i)) {
      case OP_GETGLOBAL: {
        lua_assert(ttype(&p->k[GETARG_Bc(i)]) == LUA_TSTRING);
        *name = svalue(&p->k[GETARG_Bc(i)]);
        return "global";
      }
      case OP_MOVE: {
        int a = GETARG_A(i);
        int b = GETARG_B(i);  /* move from `b' to `a' */
        if (b < a)
          return getobjname(L, ci->base+b, name);  /* get name for `b' */
        break;
      }
      case OP_GETTABLE:
      case OP_SELF: {
        int c = GETARG_C(i) - MAXSTACK;
        if (c >= 0 && ttype(&p->k[c]) == LUA_TSTRING) {
          *name = svalue(&p->k[c]);
          return "field";
        }
        break;
      }
      default: break;
    }
  }
  return NULL;  /* no useful name found */
}


static const char *getfuncname (lua_State *L, CallInfo *ci,
                                  const char **name) {
  ci = ci->prev;  /* calling function */
  if (ci == &L->basefunc || !isLmark(ci))
    return NULL;  /* not an active Lua function */
  else {
    Proto *p = ci_func(ci)->l.p;
    int pc = currentpc(ci);
    Instruction i;
    if (pc == -1) return NULL;  /* function is not activated */
    i = p->code[pc];
    return (GET_OPCODE(i) == OP_CALL
             ? getobjname(L, ci->base+GETARG_A(i), name)
             : NULL);  /* no useful name found */
  }
}


void luaG_typeerror (lua_State *L, StkId o, const char *op) {
  const char *name;
  const char *kind = getobjname(L, o, &name);
  const char *t = luaT_typenames[ttype(o)];
  if (kind)
    luaO_verror(L, "attempt to %.30s %.20s `%.40s' (a %.10s value)",
                op, kind, name, t);
  else
    luaO_verror(L, "attempt to %.30s a %.10s value", op, t);
}


void luaG_concaterror (lua_State *L, StkId p1, StkId p2) {
  if (ttype(p1) == LUA_TSTRING) p1 = p2;
  lua_assert(ttype(p1) != LUA_TSTRING);
  luaG_typeerror(L, p1, "concat");
}


void luaG_aritherror (lua_State *L, StkId p1, TObject *p2) {
  TObject temp;
  if (luaV_tonumber(p1, &temp) != NULL)
    p1 = p2;  /* first operand is OK; error is in the second */
  luaG_typeerror(L, p1, "perform arithmetic on");
}


void luaG_ordererror (lua_State *L, const TObject *p1, const TObject *p2) {
  const char *t1 = luaT_typenames[ttype(p1)];
  const char *t2 = luaT_typenames[ttype(p2)];
  if (t1[2] == t2[2])
    luaO_verror(L, "attempt to compare two %.10s values", t1);
  else
    luaO_verror(L, "attempt to compare %.10s with %.10s", t1, t2);
}

