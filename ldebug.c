/*
** $Id: ldebug.c,v 1.79 2001/06/07 14:44:51 roberto Exp roberto $
** Debug Interface
** See Copyright Notice in lua.h
*/


#include <stdlib.h>

#define LUA_PRIVATE
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



static const l_char *getfuncname (lua_State *L, CallInfo *ci,
                                  const l_char **name);



static int isLmark (CallInfo *ci) {
  lua_assert(ci == NULL || ttype(ci->base - 1) == LUA_TFUNCTION);
  return (ci && ci->prev && !ci_func(ci)->isC);
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
    return (*ci->pc - ci_func(ci)->f.l->code) - 1;
  else
    return -1;  /* function is not active */
}


static int currentline (CallInfo *ci) {
  if (!isLmark(ci))
    return -1;  /* only active lua functions have current-line information */
  else {
    int *lineinfo = ci_func(ci)->f.l->lineinfo;
    return luaG_getline(lineinfo, currentpc(ci), 1, NULL);
  }
}



static Proto *getluaproto (CallInfo *ci) {
  return (isLmark(ci) ? ci_func(ci)->f.l : NULL);
}


LUA_API const l_char *lua_getlocal (lua_State *L, const lua_Debug *ar, int n) {
  const l_char *name;
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


LUA_API const l_char *lua_setlocal (lua_State *L, const lua_Debug *ar, int n) {
  const l_char *name;
  CallInfo *ci;
  Proto *fp;
  lua_lock(L);
  name = NULL;
  ci = ar->_ci;
  fp = getluaproto(ci);
  L->top--;  /* pop new value */
  if (fp) {  /* is a Lua function? */
    name = luaF_getlocalname(fp, n, currentpc(ci));
    if (!name || name[0] == l_c('('))  /* `(' starts private locals */
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
  ar->what = l_s("Lua");
}


static void funcinfo (lua_State *L, lua_Debug *ar, StkId func) {
  Closure *cl;
  if (ttype(func) == LUA_TFUNCTION)
    cl = clvalue(func);
  else {
    luaD_error(L, l_s("value for `lua_getinfo' is not a function"));
    cl = NULL;  /* to avoid warnings */
  }
  if (cl->isC) {
    ar->source = l_s("=C");
    ar->linedefined = -1;
    ar->what = l_s("C");
  }
  else
    infoLproto(ar, cl->f.l);
  luaO_chunkid(ar->short_src, ar->source, LUA_IDSIZE);
  if (ar->linedefined == 0)
    ar->what = l_s("main");
}


static const l_char *travtagmethods (global_State *G, const TObject *o) {
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


static const l_char *travglobals (lua_State *L, const TObject *o) {
  Hash *g = L->gt;
  int i;
  for (i=0; i<g->size; i++) {
    if (luaO_equalObj(o, val(node(g, i))) &&
        ttype_key(node(g, i)) == LUA_TSTRING)
      return getstr(tsvalue_key(node(g, i)));
  }
  return NULL;
}


static void getname (lua_State *L, const TObject *f, lua_Debug *ar) {
  /* try to find a name for given function */
  if ((ar->name = travglobals(L, f)) != NULL)
    ar->namewhat = l_s("global");
  /* not found: try tag methods */
  else if ((ar->name = travtagmethods(G(L), f)) != NULL)
    ar->namewhat = l_s("tag-method");
  else ar->namewhat = l_s("");  /* not found at all */
}


LUA_API int lua_getinfo (lua_State *L, const l_char *what, lua_Debug *ar) {
  StkId f;
  CallInfo *ci;
  int status = 1;
  lua_lock(L);
  if (*what != l_c('>')) {  /* function is active? */
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
      case l_c('S'): {
        funcinfo(L, ar, f);
        break;
      }
      case l_c('l'): {
        ar->currentline = currentline(ci);
        break;
      }
      case l_c('u'): {
        ar->nups = (ttype(f) == LUA_TFUNCTION) ? clvalue(f)->nupvalues : 0;
        break;
      }
      case l_c('n'): {
        ar->namewhat = (ci) ? getfuncname(L, ci, &ar->name) : NULL;
        if (ar->namewhat == NULL)
          getname(L, f, ar);
        break;
      }
      case l_c('f'): {
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


static int checkopenop (Instruction i) {
  OpCode op = GET_OPCODE(i);
  switch (op) {
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
    switch (getOpMode(op)) {
      case iABC: {
        b = GETARG_B(i);
        c = GETARG_C(i);
        if (testOpMode(op, OpModeBreg)) {
          checkreg(pt, b);
          check(c < pt->maxstacksize ||
               (c >= MAXSTACK && c-MAXSTACK < pt->sizek));
        }
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
    if (testOpMode(op, OpModeAreg)) checkreg(pt, a);
    if (testOpMode(op, OpModesetA)) {
      if (a == reg) last = pc;  /* change register `a' */
    }
    if (testOpMode(op, OpModeT))
      check(GET_OPCODE(pt->code[pc+1]) == OP_CJMP);
    switch (op) {
      case OP_LOADNIL: {
        if (a <= reg && reg <= b)
          last = pc;  /* set registers from `a' to `b' */
        break;
      }
      case OP_LOADUPVAL: {
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
        check(b < c);  /* at least two operands */
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
      case OP_NILJMP: {
        check(pc+2 < pt->sizecode);  /* check its jump */
        break;
      }
      case OP_CALL: {
        if (b == NO_REG) b = pt->maxstacksize;
        if (c == NO_REG) {
          check(checkopenop(pt->code[pc+1]));
          c = 1;
        }
        check(b > a);
        checkreg(pt, b-1);
        checkreg(pt, a+c-1);
        if (reg >= a) last = pc;  /* affect all registers above base */
        break;
      }
      case OP_RETURN: {
        if (b == NO_REG) b = pt->maxstacksize;
        checkreg(pt, b-1);
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
        check(b < pt->sizekproto);
        checkreg(pt, a + pt->kproto[b]->nupvalues - 1);
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


static const l_char *getobjname (lua_State *L, StkId obj, const l_char **name) {
  CallInfo *ci = ci_stack(L, obj);
  if (isLmark(ci)) {  /* an active Lua function? */
    Proto *p = ci_func(ci)->f.l;
    int pc = currentpc(ci);
    int stackpos = obj - ci->base;
    Instruction i;
    *name = luaF_getlocalname(p, stackpos+1, pc);
    if (*name)  /* is a local? */
      return l_s("local");
    i = luaG_symbexec(p, pc, stackpos);  /* try symbolic execution */
    lua_assert(pc != -1);
    switch (GET_OPCODE(i)) {
      case OP_GETGLOBAL: {
        lua_assert(ttype(&p->k[GETARG_Bc(i)]) == LUA_TSTRING);
        *name = getstr(tsvalue(&p->k[GETARG_Bc(i)]));
        return l_s("global");
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
          *name = getstr(tsvalue(&p->k[c]));
          return l_s("field");
        }
        break;
      }
      default: break;
    }
  }
  return NULL;  /* no useful name found */
}


static const l_char *getfuncname (lua_State *L, CallInfo *ci,
                                  const l_char **name) {
  ci = ci->prev;  /* calling function */
  if (ci == &L->basefunc || !isLmark(ci))
    return NULL;  /* not an active Lua function */
  else {
    Proto *p = ci_func(ci)->f.l;
    int pc = currentpc(ci);
    Instruction i;
    if (pc == -1) return NULL;  /* function is not activated */
    i = p->code[pc];
    return (GET_OPCODE(i) == OP_CALL
             ? getobjname(L, ci->base+GETARG_A(i), name)
             : NULL);  /* no useful name found */
  }
}


void luaG_typeerror (lua_State *L, StkId o, const l_char *op) {
  const l_char *name;
  const l_char *kind = getobjname(L, o, &name);
  const l_char *t = luaT_typename(G(L), o);
  if (kind)
    luaO_verror(L, l_s("attempt to %.30s %.20s `%.40s' (a %.10s value)"),
                op, kind, name, t);
  else
    luaO_verror(L, l_s("attempt to %.30s a %.10s value"), op, t);
}


void luaG_concaterror (lua_State *L, StkId p1, StkId p2) {
  if (ttype(p1) == LUA_TSTRING) p1 = p2;
  lua_assert(ttype(p1) != LUA_TSTRING);
  luaG_typeerror(L, p1, l_s("concat"));
}


void luaG_aritherror (lua_State *L, StkId p1, TObject *p2) {
  TObject temp;
  if (luaV_tonumber(p1, &temp) != NULL)
    p1 = p2;  /* first operand is OK; error is in the second */
  luaG_typeerror(L, p1, l_s("perform arithmetic on"));
}


void luaG_ordererror (lua_State *L, const TObject *p1, const TObject *p2) {
  const l_char *t1 = luaT_typename(G(L), p1);
  const l_char *t2 = luaT_typename(G(L), p2);
  if (t1[2] == t2[2])
    luaO_verror(L, l_s("attempt to compare two %.10s values"), t1);
  else
    luaO_verror(L, l_s("attempt to compare %.10s with %.10s"), t1, t2);
}



#define opmode(t,a,b,c,sa,k,m) (((t)<<OpModeT) | \
   ((a)<<OpModeAreg) | ((b)<<OpModeBreg) | ((c)<<OpModeCreg) | \
   ((sa)<<OpModesetA) | ((k)<<OpModeK) | (m))


const lu_byte luaG_opmodes[] = {
/*       T A B C sA K mode		   opcode    */
  opmode(0,1,1,0, 1,0,iABC),		/* OP_MOVE */
  opmode(0,1,0,0, 1,1,iABc),		/* OP_LOADK */
  opmode(0,1,0,0, 1,0,iAsBc),		/* OP_LOADINT */
  opmode(0,1,1,0, 1,0,iABC),		/* OP_LOADNIL */
  opmode(0,1,0,0, 1,0,iABc),		/* OP_LOADUPVAL */
  opmode(0,1,0,0, 1,1,iABc),		/* OP_GETGLOBAL */
  opmode(0,1,1,1, 1,0,iABC),		/* OP_GETTABLE */
  opmode(0,1,0,0, 0,1,iABc),		/* OP_SETGLOBAL */
  opmode(0,1,1,1, 0,0,iABC),		/* OP_SETTABLE */
  opmode(0,1,0,0, 1,0,iABc),		/* OP_NEWTABLE */
  opmode(0,1,1,1, 1,0,iABC),		/* OP_SELF */
  opmode(0,1,1,1, 1,0,iABC),		/* OP_ADD */
  opmode(0,1,1,1, 1,0,iABC),		/* OP_SUB */
  opmode(0,1,1,1, 1,0,iABC),		/* OP_MUL */
  opmode(0,1,1,1, 1,0,iABC),		/* OP_DIV */
  opmode(0,1,1,1, 1,0,iABC),		/* OP_POW */
  opmode(0,1,1,0, 1,0,iABC),		/* OP_UNM */
  opmode(0,1,1,0, 1,0,iABC),		/* OP_NOT */
  opmode(0,1,1,1, 1,0,iABC),		/* OP_CONCAT */
  opmode(0,0,0,0, 0,0,iAsBc),		/* OP_JMP */
  opmode(0,0,0,0, 0,0,iAsBc),		/* OP_CJMP */
  opmode(1,0,1,1, 0,0,iABC),		/* OP_TESTEQ */
  opmode(1,0,1,1, 0,0,iABC),		/* OP_TESTNE */
  opmode(1,0,1,1, 0,0,iABC),		/* OP_TESTLT */
  opmode(1,0,1,1, 0,0,iABC),		/* OP_TESTLE */
  opmode(1,0,1,1, 0,0,iABC),		/* OP_TESTGT */
  opmode(1,0,1,1, 0,0,iABC),		/* OP_TESTGE */
  opmode(1,1,1,0, 1,0,iABC),		/* OP_TESTT */
  opmode(1,1,1,0, 1,0,iABC),		/* OP_TESTF */
  opmode(0,1,0,0, 1,0,iAsBc),		/* OP_NILJMP */
  opmode(0,1,0,0, 0,0,iABC),		/* OP_CALL */
  opmode(0,1,0,0, 0,0,iABC),		/* OP_RETURN */
  opmode(0,1,0,0, 0,0,iAsBc),		/* OP_FORPREP */
  opmode(0,1,0,0, 0,0,iAsBc),		/* OP_FORLOOP */
  opmode(0,1,0,0, 0,0,iAsBc),		/* OP_TFORPREP */
  opmode(0,1,0,0, 0,0,iAsBc),		/* OP_TFORLOOP */
  opmode(0,1,0,0, 0,0,iABc),		/* OP_SETLIST */
  opmode(0,1,0,0, 0,0,iABc),		/* OP_SETLIST0 */
  opmode(0,1,0,0, 0,0,iABc)		/* OP_CLOSURE */
};
