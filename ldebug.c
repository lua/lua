/*
** $Id: ldebug.c,v 1.66 2001/02/20 18:28:11 roberto Exp roberto $
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
  lua_Hook oldhook;
  LUA_LOCK(L);
  oldhook = L->callhook;
  L->callhook = func;
  LUA_UNLOCK(L);
  return oldhook;
}


LUA_API lua_Hook lua_setlinehook (lua_State *L, lua_Hook func) {
  lua_Hook oldhook;
  LUA_LOCK(L);
  oldhook = L->linehook;
  L->linehook = func;
  LUA_UNLOCK(L);
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
  StkId f;
  int status;
  LUA_LOCK(L);
  f = aux_stackedfunction(L, level, L->top);
  if (f == NULL) status = 0;  /* there is no such level */
  else {
    ar->_func = f;
    status = 1;
  }
  LUA_UNLOCK(L);
  return status;
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
  StkId f;
  Proto *fp;
  LUA_LOCK(L);
  name = NULL;
  f = ar->_func;
  fp = getluaproto(f);
  if (fp) {  /* `f' is a Lua function? */
    name = luaF_getlocalname(fp, n, currentpc(f));
    if (name)
      luaA_pushobject(L, (f+1)+(n-1));  /* push value */
  }
  LUA_UNLOCK(L);
  return name;
}


LUA_API const char *lua_setlocal (lua_State *L, const lua_Debug *ar, int n) {
  const char *name;
  StkId f;
  Proto *fp;
  LUA_LOCK(L);
  name = NULL;
  f = ar->_func;
  fp = getluaproto(f);
  L->top--;  /* pop new value */
  if (fp) {  /* `f' is a Lua function? */
    name = luaF_getlocalname(fp, n, currentpc(f));
    if (!name || name[0] == '(')  /* `(' starts private locals */
      name = NULL;
    else
      setobj((f+1)+(n-1), L->top);
  }
  LUA_UNLOCK(L);
  return name;
}


static void infoLproto (lua_Debug *ar, Proto *f) {
  ar->source = getstr(f->source);
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
      luaD_error(L, "value for `lua_getinfo' is not a function");
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
        ttype_key(node(g, i)) == LUA_TSTRING)
      return getstr(tsvalue_key(node(g, i)));
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
  int isactive;
  int status = 1;
  LUA_LOCK(L);
  isactive = (*what != '>');
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
      default: status = 0;  /* invalid option */
    }
  }
  if (!isactive) L->top--;  /* pop function */
  LUA_UNLOCK(L);
  return status;
}


/*
** {======================================================
** Symbolic Execution and code checker
** =======================================================
*/

#define check(x)		if (!(x)) return 0;


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


/* value for non-initialized entries in array stacklevel */
#define SL_EMPTY	255

#define checkjump(pt,sl,top,pc)	if (!checkjump_aux(pt,sl,top,pc)) return 0;

static int checkjump_aux (const Proto *pt, lu_byte *sl, int top, int pc) {
  check(0 <= pc && pc < pt->sizecode);
  if (sl == NULL) return 1;  /* not full checking */
  if (sl[pc] == SL_EMPTY)
    sl[pc] = (lu_byte)top;
  else
    check(sl[pc] == top);
  return 1;
}


static Instruction luaG_symbexec (lua_State *L, const Proto *pt,
                                  int lastpc, int stackpos) {
  int stack[MAXSTACK];  /* stores last instruction that changed a stack entry */
  lu_byte *sl = NULL;
  int top;
  int pc;
  if (stackpos < 0) {  /* full check? */
    int i;
    sl = (lu_byte *)luaO_openspace(L, pt->sizecode);
    for (i=0; i<pt->sizecode; i++)  /* initialize stack-level array */
      sl[i] = SL_EMPTY;
    check(precheck(pt));
  }
  top = pt->numparams;
  pc = 0;
  if (pt->is_vararg)  /* varargs? */
    top++;  /* `arg' */
  if (sl) sl[0] = (lu_byte)top;
  while (pc < lastpc) {
    const Instruction i = pt->code[pc++];
    OpCode op = GET_OPCODE(i);
    int arg1 = 0;
    int arg2 = 0;
    int push, pop;
    check(op < NUM_OPCODES);
    push = (int)luaK_opproperties[op].push;
    pop = (int)luaK_opproperties[op].pop;
    switch ((enum Mode)luaK_opproperties[op].mode) {
      case iO: break;
      case iU: arg1 = GETARG_U(i); check(arg1 >= 0); break;
      case iS: arg1 = GETARG_S(i); break;
      case iAB:
        arg1 = GETARG_A(i); arg2 = GETARG_B(i); check(arg1 >= 0); break;
    }
    switch (op) {
      case OP_RETURN: {
        check(arg1 <= top);
        pop = top-arg1;
        break;
      }
      case OP_CALL: {
        if (arg2 == MULT_RET) arg2 = 1;
        check(arg1 < top);
        pop = top-arg1;
        push = arg2;
        break;
      }
      case OP_TAILCALL: {
        check(arg1 < top && arg2 <= top);
        pop = top-arg2;
        break;
      }
      case OP_PUSHNIL: {
        check(arg1 > 0);
        push = arg1;
        break;
      }
      case OP_POP: {
        pop = arg1;
        break;
      }
      case OP_PUSHSTRING:
      case OP_GETGLOBAL:
      case OP_GETDOTTED:
      case OP_PUSHSELF:
      case OP_SETGLOBAL: {
        check(arg1 < pt->sizekstr);
        break;
      }
      case OP_PUSHNUM:
      case OP_PUSHNEGNUM: {
        check(arg1 < pt->sizeknum);
        break;
      }
      case OP_PUSHUPVALUE: {
        check(arg1 < pt->nupvalues);
        break;
      }
      case OP_GETLOCAL:
      case OP_GETINDEXED:
      case OP_SETLOCAL: {
        check(arg1 < top);
        break;
      }
      case OP_SETTABLE: {
        check(3 <= arg1 && arg1 <= top);
        pop = arg2;
        break;
      }
      case OP_SETLIST: {
        pop = arg2;
        check(top-pop >= 1);  /* there must be a table below the list */
        break;
      }
      case OP_SETMAP: {
        pop = 2*arg1;
        check(top-pop >= 1);
        break;
      }
      case OP_CONCAT: {
        pop = arg1;
        break;
      }
      case OP_CLOSURE: {
        check(arg1 < pt->sizekproto);
        check(arg2 == pt->kproto[arg1]->nupvalues);
        pop = arg2;
        break;
      }
      case OP_JMPNE:
      case OP_JMPEQ:
      case OP_JMPLT:
      case OP_JMPLE:
      case OP_JMPGT:
      case OP_JMPGE:
      case OP_JMPT:
      case OP_JMPF:
      case OP_JMP: {
        checkjump(pt, sl, top-pop, pc+arg1);
        break;
      }
      case OP_FORLOOP:
      case OP_LFORLOOP:
      case OP_JMPONT:
      case OP_JMPONF: {
        int newpc = pc+arg1;
        checkjump(pt, sl, top, newpc);
        /* jump is forward and do not skip `lastpc' and not full check? */
        if (pc < newpc && newpc <= lastpc && stackpos >= 0) {
          stack[top-1] = pc-1;  /* value comes from `and'/`or' */
          pc = newpc;  /* do the jump */
          pop = 0;  /* do not pop */
        }
        break;
      }
      case OP_PUSHNILJMP: {
        check(GET_OPCODE(pt->code[pc]) == OP_PUSHINT); /* only valid sequence */
        break;
      }
      case OP_FORPREP: {
        int endfor = pc-arg1-1;  /* jump is `negative' here */
        check(top >= 3);
        checkjump(pt, sl, top+push, endfor);
        check(GET_OPCODE(pt->code[endfor]) == OP_FORLOOP);
        check(GETARG_S(pt->code[endfor]) == arg1);
        break;
      }
      case OP_LFORPREP: {
        int endfor = pc-arg1-1;  /* jump is `negative' here */
        check(top >= 1);
        checkjump(pt, sl, top+push, endfor);
        check(GET_OPCODE(pt->code[endfor]) == OP_LFORLOOP);
        check(GETARG_S(pt->code[endfor]) == arg1);
        break;
      }
      case OP_PUSHINT:
      case OP_GETTABLE:
      case OP_CREATETABLE:
      case OP_ADD:
      case OP_ADDI:
      case OP_SUB:
      case OP_MULT:
      case OP_DIV:
      case OP_POW:
      case OP_MINUS:
      case OP_NOT: {
        break;
      }
    }
    top -= pop;
    check(0 <= top && top+push <= pt->maxstacksize);
    while (push--) stack[top++] = pc-1;
    checkjump(pt, sl, top, pc);
  }
  return (stackpos >= 0) ? pt->code[stack[stackpos]] : 1;
}

/* }====================================================== */


int luaG_checkcode (lua_State *L, const Proto *pt) {
  return luaG_symbexec(L, pt, pt->sizecode-1, -1);
}


static const char *getobjname (lua_State *L, StkId obj, const char **name) {
  StkId func = aux_stackedfunction(L, 0, obj);
  if (!isLmark(func))
    return NULL;  /* not an active Lua function */
  else {
    Proto *p = infovalue(func)->func->f.l;
    int pc = currentpc(func);
    int stackpos = obj - (func+1);  /* func+1 == function base */
    Instruction i = luaG_symbexec(L, p, pc, stackpos);
    lua_assert(pc != -1);
    switch (GET_OPCODE(i)) {
      case OP_GETGLOBAL: {
        *name = getstr(p->kstr[GETARG_U(i)]);
        return "global";
      }
      case OP_GETLOCAL: {
        *name = luaF_getlocalname(p, GETARG_U(i)+1, pc);
        lua_assert(*name);
        return "local";
      }
      case OP_PUSHSELF:
      case OP_GETDOTTED: {
        *name = getstr(p->kstr[GETARG_U(i)]);
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


void luaG_typeerror (lua_State *L, StkId o, const char *op) {
  const char *name;
  const char *kind = getobjname(L, o, &name);
  const char *t = luaT_typename(G(L), o);
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


void luaG_ordererror (lua_State *L, const TObject *p1, const TObject *p2) {
  const char *t1 = luaT_typename(G(L), p1);
  const char *t2 = luaT_typename(G(L), p2);
  if (t1[2] == t2[2])
    luaO_verror(L, "attempt to compare two %.10s values", t1);
  else
    luaO_verror(L, "attempt to compare %.10s with %.10s", t1, t2);
}

