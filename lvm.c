/*
** $Id: lvm.c,v 1.178 2001/04/06 18:25:00 roberto Exp roberto $
** Lua virtual machine
** See Copyright Notice in lua.h
*/


#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LUA_PRIVATE
#include "lua.h"

#include "lapi.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lvm.h"



const TObject *luaV_tonumber (const TObject *obj, TObject *n) {
  if (ttype(obj) == LUA_TNUMBER) return obj;
  if (ttype(obj) == LUA_TSTRING && luaO_str2d(svalue(obj), &nvalue(n))) {
    ttype(n) = LUA_TNUMBER;
    return n;
  }
  else
    return NULL;
}


int luaV_tostring (lua_State *L, TObject *obj) {
  if (ttype(obj) != LUA_TNUMBER)
    return 1;
  else {
    l_char s[32];  /* 16 digits, sign, point and \0  (+ some extra...) */
    lua_number2str(s, nvalue(obj));  /* convert `s' to number */
    setsvalue(obj, luaS_new(L, s));
    return 0;
  }
}


static void traceexec (lua_State *L, lua_Hook linehook) {
  CallInfo *ci = L->ci;
  int *lineinfo = ci_func(ci)->f.l->lineinfo;
  int pc = (*ci->pc - ci_func(ci)->f.l->code) - 1;
  int newline;
  if (pc == 0) {  /* may be first time? */
    ci->line = 1;
    ci->refi = 0;
    ci->lastpc = pc+1;  /* make sure it will call linehook */
  }
  newline = luaG_getline(lineinfo, pc, ci->line, &ci->refi);
  /* calls linehook when enters a new line or jumps back (loop) */
  if (newline != ci->line || pc <= ci->lastpc) {
    ci->line = newline;
    luaD_lineHook(L, newline, linehook);
  }
  ci->lastpc = pc;
}


static Closure *luaV_closure (lua_State *L, int nelems) {
  Closure *c = luaF_newclosure(L, nelems);
  L->top -= nelems;
  while (nelems--)
    setobj(&c->upvalue[nelems], L->top+nelems);
  setclvalue(L->top, c);
  incr_top;
  return c;
}


void luaV_Cclosure (lua_State *L, lua_CFunction c, int nelems) {
  Closure *cl = luaV_closure(L, nelems);
  cl->f.c = c;
  cl->isC = 1;
}


void luaV_Lclosure (lua_State *L, Proto *l, int nelems) {
  Closure *cl = luaV_closure(L, nelems);
  cl->f.l = l;
  cl->isC = 0;
}


static void callTM (lua_State *L, const l_char *fmt, ...) {
  va_list argp;
  StkId base = L->top;
  int has_result = 0;
  va_start(argp, fmt);
  while (*fmt) {
    switch (*fmt++) {
      case l_c('c'):
        setclvalue(L->top, va_arg(argp, Closure *));
        break;
      case l_c('o'):
        setobj(L->top, va_arg(argp, TObject *));
        break;
      case l_c('s'):
        setsvalue(L->top, va_arg(argp, TString *));
        break;
      case l_c('r'):
        has_result = 1;
        continue;
    }
    incr_top;
  }
  luaD_call(L, base, has_result);
  if (has_result) {
    L->top--;
    setobj(va_arg(argp, TObject *), L->top);
  }
  va_end(argp);
}


/*
** Function to index a table.
** Receives the table at `t' and the key at the `key'.
** leaves the result at `res'.
*/
void luaV_gettable (lua_State *L, StkId t, TObject *key, StkId res) {
  Closure *tm;
  if (ttype(t) == LUA_TTABLE) {  /* `t' is a table? */
    int tg = hvalue(t)->htag;
    if (tg == LUA_TTABLE ||  /* with default tag? */
       (tm = luaT_gettm(G(L), tg, TM_GETTABLE)) == NULL) {  /* or no TM? */
      const TObject *h = luaH_get(hvalue(t), key);  /* do a primitive get */
      /* result is no nil or there is no `index' tag method? */
      if (ttype(h) != LUA_TNIL ||  /* no nil? */
         ((tm=luaT_gettm(G(L), tg, TM_INDEX)) == NULL)) {  /* or no index TM? */
        setobj(res, h);  /* default get */
        return;
      }
    }
    /* else will call the tag method */
  } else {  /* not a table; try a `gettable' tag method */
    tm = luaT_gettmbyObj(G(L), t, TM_GETTABLE);
    if (tm == NULL)  /* no tag method? */
      luaG_typeerror(L, t, l_s("index"));
  }
  callTM(L, l_s("coor"), tm, t, key, res);
}



/*
** Receives table at `t', key at `key' and value at `val'.
*/
void luaV_settable (lua_State *L, StkId t, TObject *key, StkId val) {
  Closure *tm;
  if (ttype(t) == LUA_TTABLE) {  /* `t' is a table? */
    int tg = hvalue(t)->htag;
    if (hvalue(t)->htag == LUA_TTABLE ||  /* with default tag? */
        (tm = luaT_gettm(G(L), tg, TM_SETTABLE)) == NULL) { /* or no TM? */
      setobj(luaH_set(L, hvalue(t), key), val);  /* do a primitive set */
      return;
    }
    /* else will call the tag method */
  } else {  /* not a table; try a `settable' tag method */
    tm = luaT_gettmbyObj(G(L), t, TM_SETTABLE);
    if (tm == NULL)  /* no tag method? */
      luaG_typeerror(L, t, l_s("index"));
  }
  callTM(L, l_s("cooo"), tm, t, key, val);
}


void luaV_getglobal (lua_State *L, TString *name, StkId res) {
  const TObject *value = luaH_getstr(L->gt, name);
  Closure *tm;
  if (!HAS_TM_GETGLOBAL(L, ttype(value)) ||  /* is there a tag method? */
      (tm = luaT_gettmbyObj(G(L), value, TM_GETGLOBAL)) == NULL) {
    setobj(res, value);  /* default behavior */
  } else
    callTM(L, l_s("csor"), tm, name, value, res);
}


void luaV_setglobal (lua_State *L, TString *name, StkId val) {
  TObject *oldvalue = luaH_setstr(L, L->gt, name);
  Closure *tm;
  if (!HAS_TM_SETGLOBAL(L, ttype(oldvalue)) ||  /* no tag methods? */
     (tm = luaT_gettmbyObj(G(L), oldvalue, TM_SETGLOBAL)) == NULL) {
    setobj(oldvalue, val);  /* raw set */
  } else
    callTM(L, l_s("csoo"), tm, name, oldvalue, val);
}


static int call_binTM (lua_State *L, const TObject *p1, const TObject *p2,
                       TObject *res, TMS event) {
  TString *opname;
  Closure *tm = luaT_gettmbyObj(G(L), p1, event);  /* try first operand */
  if (tm == NULL) {
    tm = luaT_gettmbyObj(G(L), p2, event);  /* try second operand */
    if (tm == NULL) {
      tm = luaT_gettm(G(L), 0, event);  /* try a `global' method */
      if (tm == NULL)
        return 0;  /* no tag method */
    }
  }
  opname = luaS_new(L, luaT_eventname[event]);
  callTM(L, l_s("coosr"), tm, p1, p2, opname, res);
  return 1;
}


static void call_arith (lua_State *L, StkId p1, TObject *p2,
                        StkId res, TMS event) {
  if (!call_binTM(L, p1, p2, res, event))
    luaG_aritherror(L, p1, p2);
}


static int luaV_strlessthan (const TString *ls, const TString *rs) {
  const l_char *l = getstr(ls);
  size_t ll = ls->len;
  const l_char *r = getstr(rs);
  size_t lr = rs->len;
  for (;;) {
    int temp = strcoll(l, r);
    if (temp != 0) return (temp < 0);
    else {  /* strings are equal up to a `\0' */
      size_t len = strlen(l);  /* index of first `\0' in both strings */
      if (len == lr)  /* r is finished? */
        return 0;  /* l is equal or greater than r */
      else if (len == ll)  /* l is finished? */
        return 1;  /* l is smaller than r (because r is not finished) */
      /* both strings longer than `len'; go on comparing (after the `\0') */
      len++;
      l += len; ll -= len; r += len; lr -= len;
    }
  }
}


int luaV_lessthan (lua_State *L, const TObject *l, const TObject *r) {
  if (ttype(l) == LUA_TNUMBER && ttype(r) == LUA_TNUMBER)
    return (nvalue(l) < nvalue(r));
  else if (ttype(l) == LUA_TSTRING && ttype(r) == LUA_TSTRING)
    return luaV_strlessthan(tsvalue(l), tsvalue(r));
  else {  /* try TM */
    if (!call_binTM(L, l, r, L->top, TM_LT))
      luaG_ordererror(L, l, r);
    return (ttype(L->top) != LUA_TNIL);
  }
}


void luaV_strconc (lua_State *L, int total, StkId top) {
  do {
    int n = 2;  /* number of elements handled in this pass (at least 2) */
    if (tostring(L, top-2) || tostring(L, top-1)) {
      if (!call_binTM(L, top-2, top-1, top-2, TM_CONCAT))
        luaG_concaterror(L, top-2, top-1);
    } else if (tsvalue(top-1)->len > 0) {  /* if len=0, do nothing */
      /* at least two string values; get as many as possible */
      lu_mem tl = (lu_mem)tsvalue(top-1)->len + (lu_mem)tsvalue(top-2)->len;
      l_char *buffer;
      int i;
      while (n < total && !tostring(L, top-n-1)) {  /* collect total length */
        tl += tsvalue(top-n-1)->len;
        n++;
      }
      if (tl > MAX_SIZET) luaD_error(L, l_s("string size overflow"));
      buffer = luaO_openspace(L, tl, l_char);
      tl = 0;
      for (i=n; i>0; i--) {  /* concat all strings */
        size_t l = tsvalue(top-i)->len;
        memcpy(buffer+tl, svalue(top-i), l);
        tl += l;
      }
      setsvalue(top-n, luaS_newlstr(L, buffer, tl));
    }
    total -= n-1;  /* got `n' strings to create 1 new */
    top -= n-1;
  } while (total > 1);  /* repeat until only 1 result left */
}


static void luaV_pack (lua_State *L, StkId firstelem) {
  int i;
  Hash *htab = luaH_new(L, 0);
  TObject *n;
  for (i=0; firstelem+i<L->top; i++)
    setobj(luaH_setnum(L, htab, i+1), firstelem+i);
  /* store counter in field `n' */
  n = luaH_setstr(L, htab, luaS_newliteral(L, l_s("n")));
  setnvalue(n, i);
  L->top = firstelem;  /* remove elements from the stack */
  sethvalue(L->top, htab);
  incr_top;
}


static void adjust_varargs (lua_State *L, StkId base, int nfixargs) {
  int nvararg = (L->top-base) - nfixargs;
  if (nvararg < 0)
    luaD_adjusttop(L, base, nfixargs);
  luaV_pack(L, base+nfixargs);
}



/*
** some macros for common tasks in `luaV_execute'
*/

#define runtime_check(L, c)	{ if (!(c)) return L->top; }

#define RA(i)	(base+GETARG_A(i))
#define RB(i)	(base+GETARG_B(i))
#define RC(i)	(base+GETARG_C(i))
#define RKC(i)	((GETARG_C(i) < MAXSTACK) ? \
			base+GETARG_C(i) : \
			tf->k+GETARG_C(i)-MAXSTACK)
#define KBc(i)	(tf->k+GETARG_Bc(i))

#define Arith(op, optm)	{ \
  const TObject *b = RB(i); const TObject *c = RKC(i);		\
  TObject tempb, tempc; \
  if ((ttype(b) == LUA_TNUMBER || (b = luaV_tonumber(b, &tempb)) != NULL) && \
      (ttype(c) == LUA_TNUMBER || (c = luaV_tonumber(c, &tempc)) != NULL)) { \
    setnvalue(RA(i), nvalue(b) op nvalue(c));		\
  } else		\
    call_arith(L, RB(i), RKC(i), RA(i), optm); \
}


#define dojump(pc, i)	((pc) += GETARG_sBc(i))

/*
** Executes the given Lua function. Parameters are between [base,top).
** Returns n such that the the results are between [n,top).
*/
StkId luaV_execute (lua_State *L, const Closure *cl, StkId base) {
  const Proto *const tf = cl->f.l;
  const Instruction *pc;
  lua_Hook linehook;
  if (tf->is_vararg)  /* varargs? */
    adjust_varargs(L, base, tf->numparams);
  luaD_adjusttop(L, base, tf->maxstacksize);
  pc = tf->code;
  L->ci->pc = &pc;
  linehook = L->linehook;
  /* main loop of interpreter */
  for (;;) {
    const Instruction i = *pc++;
    if (linehook)
      traceexec(L, linehook);
    switch (GET_OPCODE(i)) {
      case OP_MOVE: {
        setobj(RA(i), RB(i));
        break;
      }
      case OP_LOADK: {
        setobj(RA(i), KBc(i));
        break;
      }
      case OP_LOADINT: {
        setnvalue(RA(i), (lua_Number)GETARG_sBc(i));
        break;
      }
      case OP_LOADUPVAL: {
        setobj(RA(i), cl->upvalue+GETARG_Bc(i));
        break;
      }
      case OP_LOADNIL: {
        TObject *ra = RA(i);
        TObject *rb = RB(i);
        do {
          setnilvalue(ra++);
        } while (ra <= rb);
        break;
      }
      case OP_GETGLOBAL: {
        lua_assert(ttype(KBc(i)) == LUA_TSTRING);
        luaV_getglobal(L, tsvalue(KBc(i)), RA(i));
        break;
      }
      case OP_GETTABLE: {
        luaV_gettable(L, RB(i), RKC(i), RA(i));
        break;
      }
      case OP_SETGLOBAL: {
        lua_assert(ttype(KBc(i)) == LUA_TSTRING);
        luaV_setglobal(L, tsvalue(KBc(i)), RA(i));
        break;
      }
      case OP_SETTABLE: {
        luaV_settable(L, RB(i), RKC(i), RA(i));
        break;
      }
      case OP_NEWTABLE: {
        luaC_checkGC(L);
        sethvalue(RA(i), luaH_new(L, GETARG_Bc(i)));
        break;
      }
      case OP_SELF: {
        StkId ra = RA(i);
        StkId rb = RB(i);
        setobj(ra+1, rb);
        luaV_gettable(L, rb, RKC(i), ra);
        break;
      }
      case OP_ADD: {
        Arith( + , TM_ADD);
        break;
      }
      case OP_SUB: {
        Arith( - , TM_SUB);
        break;
      }
      case OP_MUL: {
        Arith( * , TM_MUL);
        break;
      }
      case OP_DIV: {
        Arith( / , TM_DIV);
        break;
      }
      case OP_POW: {
        call_arith(L, RB(i), RKC(i), RA(i), TM_POW);
        break;
      }
      case OP_UNM: {
        const TObject *rb = RB(i);
        StkId ra = RA(i);
        if (ttype(rb) == LUA_TNUMBER || (rb=luaV_tonumber(rb, ra)) != NULL) {
          setnvalue(ra, -nvalue(rb));
        }
        else {
          TObject temp;
          setnilvalue(&temp);
          call_arith(L, RB(i), &temp, ra, TM_UNM);
        }
        break;
      }
      case OP_NOT: {
        if (ttype(RB(i)) == LUA_TNIL) {
          setnvalue(RA(i), 1);
        } else {
          setnilvalue(RA(i));
        }
        break;
      }
      case OP_CONCAT: {
        StkId top = RC(i)+1;
        StkId rb = RB(i);
        luaV_strconc(L, top-rb, top);
        setobj(RA(i), rb);
        luaC_checkGC(L);
        break;
      }
      case OP_CJMP:
      case OP_JMP: {
        dojump(pc, i);
        break;
      }
      case OP_TESTEQ: {
        lua_assert(GET_OPCODE(*pc) == OP_CJMP);
        if (luaO_equalObj(RB(i), RKC(i))) dojump(pc, *pc);
        pc++;
        break;
      }
      case OP_TESTNE: {
        lua_assert(GET_OPCODE(*pc) == OP_CJMP);
        if (!luaO_equalObj(RB(i), RKC(i))) dojump(pc, *pc);
        pc++;
        break;
      }
      case OP_TESTLT: {
        lua_assert(GET_OPCODE(*pc) == OP_CJMP);
        if (luaV_lessthan(L, RB(i), RKC(i))) dojump(pc, *pc);
        pc++;
        break;
      }
      case OP_TESTLE: {  /* b <= c  ===  !(c<b) */
        lua_assert(GET_OPCODE(*pc) == OP_CJMP);
        if (!luaV_lessthan(L, RKC(i), RB(i))) dojump(pc, *pc);
        pc++;
        break;
      }
      case OP_TESTGT: {  /* b > c  ===  (c<b) */
        lua_assert(GET_OPCODE(*pc) == OP_CJMP);
        if (luaV_lessthan(L, RKC(i), RB(i))) dojump(pc, *pc);
        pc++;
        break;
      }
      case OP_TESTGE: {  /* b >= c  === !(b<c) */
        lua_assert(GET_OPCODE(*pc) == OP_CJMP);
        if (!luaV_lessthan(L, RB(i), RKC(i))) dojump(pc, *pc);
        pc++;
        break;
      }
      case OP_TESTT: {
        StkId rb = RB(i);
        lua_assert(GET_OPCODE(*pc) == OP_CJMP);
        if (ttype(rb) != LUA_TNIL) {
          int a = GETARG_A(i);
          if (a != NO_REG) setobj(base+a, rb);
          dojump(pc, *pc);
        }
        pc++;
        break;
      }
      case OP_TESTF: {
        lua_assert(GET_OPCODE(*pc) == OP_CJMP);
        if (ttype(RB(i)) == LUA_TNIL) {
          int a = GETARG_A(i);
          if (a != NO_REG) setnilvalue(base+a);
          dojump(pc, *pc);
        }
        pc++;
        break;
      }
      case OP_NILJMP: {
        setnilvalue(RA(i));
        pc++;
        break;
      }
      case OP_CALL: {
        int nres;
        int b = GETARG_B(i);
        if (b != NO_REG)
          L->top = base+b;
        nres = GETARG_C(i);
        if (nres == NO_REG) nres = LUA_MULTRET;
        luaD_call(L, RA(i), nres);
        if (nres != LUA_MULTRET) {
          lua_assert(L->top == RA(i)+nres);
          L->top = base+tf->maxstacksize;
        }
        break;
      }
      case OP_RETURN: {
        int b = GETARG_B(i);
        if (b != NO_REG)
          L->top = base+b;
        return RA(i);
      }
      case OP_FORPREP: {
        int jmp = GETARG_sBc(i);
        StkId breg = RA(i);
        if (luaV_tonumber(breg, breg) == NULL)
          luaD_error(L, l_s("`for' initial value must be a number"));
        if (luaV_tonumber(breg+1, breg+1) == NULL)
          luaD_error(L, l_s("`for' limit must be a number"));
        if (luaV_tonumber(breg+2, breg+2) == NULL)
          luaD_error(L, l_s("`for' step must be a number"));
        pc += -jmp;  /* `jump' to loop end (delta is negated here) */
        nvalue(breg) -= nvalue(breg+2);/* decrement index (to be incremented) */
        /* go through */
      }
      case OP_FORLOOP: {
        StkId breg = RA(i);
        if (ttype(breg) != LUA_TNUMBER)
          luaD_error(L, l_s("`for' index must be a number"));
        runtime_check(L, ttype(breg+1) == LUA_TNUMBER &&
                         ttype(breg+2) == LUA_TNUMBER);
        nvalue(breg) += nvalue(breg+2);  /* increment index */
        if (nvalue(breg+2) > 0 ?
            nvalue(breg) <= nvalue(breg+1) :
            nvalue(breg) >= nvalue(breg+1))
          dojump(pc, i);  /* repeat loop */
        break;
      }
      case OP_TFORPREP: {
        int jmp = GETARG_sBc(i);
        StkId breg = RA(i);
        if (ttype(breg) != LUA_TTABLE)
          luaD_error(L, l_s("`for' table must be a table"));
        setnvalue(breg+1, -1);  /* initial index */
        setnilvalue(breg+2);
        setnilvalue(breg+3);
        pc += -jmp;  /* `jump' to loop end (delta is negated here) */
        /* go through */
      }
      case OP_TFORLOOP: {
        StkId breg = RA(i);
        Hash *t;
        int n;
        runtime_check(L, ttype(breg) == LUA_TTABLE);
        runtime_check(L, ttype(breg+1) == LUA_TNUMBER);
        t = hvalue(breg);
        n = (int)nvalue(breg+1);
        n = luaH_nexti(t, n);
        if (n != -1) {  /* repeat loop? */
          Node *node = node(t, n);
          setnvalue(breg+1, n);  /* index */
          setkey2obj(breg+2, node);
          setobj(breg+3, val(node));
          dojump(pc, i);  /* repeat loop */
        }
        break;
      }
      case OP_SETLIST:
      case OP_SETLISTO: {
        int bc;
        int n;
        Hash *h;
        StkId ra = RA(i);
        runtime_check(L, ttype(ra) == LUA_TTABLE);
        h = hvalue(ra);
        bc = GETARG_Bc(i);
        if (GET_OPCODE(i) == OP_SETLIST)
          n = (bc&(LFIELDS_PER_FLUSH-1)) + 1;
        else
          n = L->top - ra - 1;
        bc &= ~(LFIELDS_PER_FLUSH-1);  /* bc = bc - bc%FPF */
        for (; n > 0; n--)
          setobj(luaH_setnum(L, h, bc+n), ra+n);
        break;
      }
      case OP_CLOSURE: {
        Proto *p = tf->kproto[GETARG_Bc(i)];
        int nup = p->nupvalues;
        StkId ra = RA(i);
        L->top = ra+nup;
        luaV_Lclosure(L, p, nup);
        L->top = base+tf->maxstacksize;
        luaC_checkGC(L);
        break;
      }
    }
  }
}

