/*
** $Id: lvm.c,v 1.1 2001/11/29 22:14:34 rieru Exp rieru $
** Lua virtual machine
** See Copyright Notice in lua.h
*/


#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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



static void luaV_checkGC (lua_State *L, StkId top) {
  if (G(L)->nblocks >= G(L)->GCthreshold) {
    StkId temp = L->top;
    L->top = top;
    luaC_collectgarbage(L);
    L->top = temp;  /* restore old top position */
  }
}


const TObject *luaV_tonumber (const TObject *obj, TObject *n) {
  lua_Number num;
  if (ttype(obj) == LUA_TNUMBER) return obj;
  if (ttype(obj) == LUA_TSTRING && luaO_str2d(svalue(obj), &num)) {
    setnvalue(n, num);
    return n;
  }
  else
    return NULL;
}


int luaV_tostring (lua_State *L, TObject *obj) {
  if (ttype(obj) != LUA_TNUMBER)
    return 1;
  else {
    char s[32];  /* 16 digits, sign, point and \0  (+ some extra...) */
    lua_number2str(s, nvalue(obj));  /* convert `s' to number */
    setsvalue(obj, luaS_new(L, s));
    return 0;
  }
}


static void traceexec (lua_State *L, lua_Hook linehook) {
  CallInfo *ci = L->ci;
  int *lineinfo = ci_func(ci)->l.p->lineinfo;
  int pc = (*ci->pc - ci_func(ci)->l.p->code) - 1;
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



/* maximum stack used by a call to a tag method (func + args) */
#define MAXSTACK_TM	4

static void callTM (lua_State *L, const TObject *f,
     const TObject *p1, const TObject *p2, const TObject *p3, TObject *result ) {
  StkId base = L->top;
  luaD_checkstack(L, MAXSTACK_TM);
  setobj(base, f);  /* push function */
  setobj(base+1, p1);  /* 1st argument */
  setobj(base+2, p2);  /* 2nd argument */
  L->top += 3;
  if (p3) {
    setobj(base+3, p3);  /* 3th argument */
    L->top++;
  }
  luaD_call(L, base);
  if (result) {  /* need a result? */
    if (L->top == base) {  /* are there valid results? */
      setnilvalue(result);  /* function had no results */
    }
    else {
      setobj(result, base);  /* get first result */
    }
  }
  L->top = base;  /* restore top */
}



/*
** Function to index a table.
** Receives the table at `t' and the key at `key'.
** leaves the result at `res'.
*/
void luaV_gettable (lua_State *L, StkId t, TObject *key, StkId res) {
  const TObject *tm;
  init:
  if (ttype(t) == LUA_TTABLE) {  /* `t' is a table? */
    Table *et = hvalue(t)->eventtable;
    if ((tm = fasttm(L, et, TM_GETTABLE)) == NULL) {  /* no gettable TM? */
      const TObject *h = luaH_get(hvalue(t), key);  /* do a primitive get */
      /* result is no nil or there is no `index' tag method? */
      if (ttype(h) != LUA_TNIL ||  /* no nil? */
          (tm = fasttm(L, et, TM_INDEX)) == NULL) {  /* or no index TM? */
        setobj(res, h);  /* default get */
        return;
      }
    }
    /* else will call the tag method */
  } else {  /* not a table; try a `gettable' tag method */
    if (ttype(tm = luaT_gettmbyobj(L, t, TM_GETTABLE)) == LUA_TNIL) {
      luaG_typeerror(L, t, "index");
      return;  /* to avoid warnings */
    }
  }
  if (ttype(tm) == LUA_TFUNCTION)
    callTM(L, tm, t, key, NULL, res);
  else {
    t = tm;
    goto init;  /* return luaV_gettable(L, tm, key, res); */
  }
}



/*
** Receives table at `t', key at `key' and value at `val'.
*/
void luaV_settable (lua_State *L, StkId t, TObject *key, StkId val) {
  const TObject *tm;
  init:
  if (ttype(t) == LUA_TTABLE) {  /* `t' is a table? */
    Table *et = hvalue(t)->eventtable;
    if ((tm = fasttm(L, et, TM_SETTABLE)) == NULL) {  /* no TM? */
      luaH_set(L, hvalue(t), key, val);  /* do a primitive set */
      return;
    }
    /* else will call the tag method */
  } else {  /* not a table; try a `settable' tag method */
    if (ttype(tm = luaT_gettmbyobj(L, t, TM_SETTABLE)) == LUA_TNIL) {
      luaG_typeerror(L, t, "index");
      return;  /* to avoid warnings */
    }
  }
  if (ttype(tm) == LUA_TFUNCTION)
    callTM(L, tm, t, key, val, NULL);
  else {
    t = tm;
    goto init;  /* luaV_settable(L, tm, key, val); */
  }
}


static int call_binTM (lua_State *L, const TObject *p1, const TObject *p2,
                       TObject *res, TMS event) {
  const TObject *tm = luaT_gettmbyobj(L, p1, event);  /* try first operand */
  if (ttype(tm) == LUA_TNIL)
    tm = luaT_gettmbyobj(L, p2, event);  /* try second operand */
  if (ttype(tm) != LUA_TFUNCTION) return 0;
  callTM(L, tm, p1, p2, NULL, res);
  return 1;
}


static void call_arith (lua_State *L, StkId p1, TObject *p2,
                        StkId res, TMS event) {
  if (!call_binTM(L, p1, p2, res, event))
    luaG_aritherror(L, p1, p2);
}


static int luaV_strlessthan (const TString *ls, const TString *rs) {
  const char *l = getstr(ls);
  size_t ll = ls->tsv.len;
  const char *r = getstr(rs);
  size_t lr = rs->tsv.len;
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
  luaV_checkGC(L, top);
  do {
    int n = 2;  /* number of elements handled in this pass (at least 2) */
    if (tostring(L, top-2) || tostring(L, top-1)) {
      if (!call_binTM(L, top-2, top-1, top-2, TM_CONCAT))
        luaG_concaterror(L, top-2, top-1);
    } else if (tsvalue(top-1)->tsv.len > 0) {  /* if len=0, do nothing */
      /* at least two string values; get as many as possible */
      lu_mem tl = cast(lu_mem, tsvalue(top-1)->tsv.len) +
                  cast(lu_mem, tsvalue(top-2)->tsv.len);
      char *buffer;
      int i;
      while (n < total && !tostring(L, top-n-1)) {  /* collect total length */
        tl += tsvalue(top-n-1)->tsv.len;
        n++;
      }
      if (tl > MAX_SIZET) luaD_error(L, "string size overflow");
      buffer = luaO_openspace(L, tl, char);
      tl = 0;
      for (i=n; i>0; i--) {  /* concat all strings */
        size_t l = tsvalue(top-i)->tsv.len;
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
  Table *htab = luaH_new(L, 0, 0);
  TObject n, nname;
  for (i=0; firstelem+i<L->top; i++)
    luaH_setnum(L, htab, i+1, firstelem+i);
  /* store counter in field `n' */
  setnvalue(&n, i);
  setsvalue(&nname, luaS_newliteral(L, "n"));
  luaH_set(L, htab, &nname, &n);
  L->top = firstelem;  /* remove elements from the stack */
  sethvalue(L->top, htab);
  incr_top;
}


static void adjust_varargs (lua_State *L, StkId base, int nfixargs) {
  int nvararg = (L->top-base) - nfixargs;
  StkId firstvar = base + nfixargs;  /* position of first vararg */
  if (nvararg < 0) {
    luaD_checkstack(L, -nvararg);
    luaD_adjusttop(L, firstvar);
  }
  luaV_pack(L, firstvar);
}


static void powOp (lua_State *L, StkId ra, StkId rb, StkId rc) {
  const TObject *b = rb;
  const TObject *c = rc;
  TObject tempb, tempc;
  if ((b = luaV_tonumber(b, &tempb)) != NULL &&
      (c = luaV_tonumber(c, &tempc)) != NULL) {
    TObject o, f;
    setsvalue(&o, luaS_newliteral(L, "pow"));
    luaV_gettable(L, gt(L), &o, &f);
    if (ttype(&f) != LUA_TFUNCTION)
      luaD_error(L, "`pow' (for `^' operator) is not a function");
    callTM(L, &f, b, c, NULL, ra);
  }
  else
    call_arith(L, rb, rc, ra, TM_POW);
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
    setnvalue(ra, nvalue(b) op nvalue(c));		\
  } else		\
    call_arith(L, RB(i), RKC(i), ra, optm); \
}


#define dojump(pc, i)	((pc) += GETARG_sBc(i))

/*
** Executes the given Lua function. Parameters are between [base,top).
** Returns n such that the the results are between [n,top).
*/
StkId luaV_execute (lua_State *L, const LClosure *cl, StkId base) {
  const Proto *const tf = cl->p;
  const Instruction *pc;
  lua_Hook linehook;
  if (tf->is_vararg)  /* varargs? */
    adjust_varargs(L, base, tf->numparams);
  if (base > L->stack_last - tf->maxstacksize)
    luaD_stackerror(L);
  luaD_adjusttop(L, base + tf->maxstacksize);
  pc = tf->code;
  L->ci->pc = &pc;
  linehook = L->linehook;
  /* main loop of interpreter */
  for (;;) {
    const Instruction i = *pc++;
    const StkId ra = RA(i);
    if (linehook)
      traceexec(L, linehook);
    switch (GET_OPCODE(i)) {
      case OP_MOVE: {
        setobj(ra, RB(i));
        break;
      }
      case OP_LOADK: {
        setobj(ra, KBc(i));
        break;
      }
      case OP_LOADINT: {
        setnvalue(ra, (lua_Number)GETARG_sBc(i));
        break;
      }
      case OP_LOADNIL: {
        TObject *rb = RB(i);
        do {
          setnilvalue(rb--);
        } while (rb >= ra);
        break;
      }
      case OP_GETUPVAL: {
        int b = GETARG_B(i);
        setobj(ra, cl->upvals[b]->v);
        break;
      }
      case OP_GETGLOBAL: {
        lua_assert(ttype(KBc(i)) == LUA_TSTRING);
        luaV_gettable(L, gt(L), KBc(i), ra);
        break;
      }
      case OP_GETTABLE: {
        luaV_gettable(L, RB(i), RKC(i), ra);
        break;
      }
      case OP_SETGLOBAL: {
        lua_assert(ttype(KBc(i)) == LUA_TSTRING);
        luaV_settable(L, gt(L), KBc(i), ra);
        break;
      }
      case OP_SETUPVAL: {
        int b = GETARG_B(i);
        setobj(cl->upvals[b]->v, ra);
        break;
      }
      case OP_SETTABLE: {
        luaV_settable(L, RB(i), RKC(i), ra);
        break;
      }
      case OP_NEWTABLE: {
        int b = GETARG_B(i);
        if (b > 0) b = twoto(b-1);
        sethvalue(ra, luaH_new(L, b, GETARG_C(i)));
        luaV_checkGC(L, ra+1);
        break;
      }
      case OP_SELF: {
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
        powOp(L, ra, RB(i), RKC(i));
        break;
      }
      case OP_UNM: {
        const TObject *rb = RB(i);
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
          setnvalue(ra, 1);
        } else {
          setnilvalue(ra);
        }
        break;
      }
      case OP_CONCAT: {
        StkId top = RC(i)+1;
        StkId rb = RB(i);
        luaV_strconc(L, top-rb, top);
        setobj(ra, rb);
        break;
      }
      case OP_CJMP:
      case OP_JMP: {
        dojump(pc, i);
        break;
      }
      case OP_TESTEQ: {
        lua_assert(GET_OPCODE(*pc) == OP_CJMP);
        if (luaO_equalObj(ra, RKC(i))) dojump(pc, *pc);
        pc++;
        break;
      }
      case OP_TESTNE: {
        lua_assert(GET_OPCODE(*pc) == OP_CJMP);
        if (!luaO_equalObj(ra, RKC(i))) dojump(pc, *pc);
        pc++;
        break;
      }
      case OP_TESTLT: {
        lua_assert(GET_OPCODE(*pc) == OP_CJMP);
        if (luaV_lessthan(L, ra, RKC(i))) dojump(pc, *pc);
        pc++;
        break;
      }
      case OP_TESTLE: {  /* b <= c  ===  !(c<b) */
        lua_assert(GET_OPCODE(*pc) == OP_CJMP);
        if (!luaV_lessthan(L, RKC(i), ra)) dojump(pc, *pc);
        pc++;
        break;
      }
      case OP_TESTGT: {  /* b > c  ===  (c<b) */
        lua_assert(GET_OPCODE(*pc) == OP_CJMP);
        if (luaV_lessthan(L, RKC(i), ra)) dojump(pc, *pc);
        pc++;
        break;
      }
      case OP_TESTGE: {  /* b >= c  === !(b<c) */
        lua_assert(GET_OPCODE(*pc) == OP_CJMP);
        if (!luaV_lessthan(L, ra, RKC(i))) dojump(pc, *pc);
        pc++;
        break;
      }
      case OP_TESTT: {
        StkId rb = RB(i);
        lua_assert(GET_OPCODE(*pc) == OP_CJMP);
        if (ttype(rb) != LUA_TNIL) {
          setobj(ra, rb);
          dojump(pc, *pc);
        }
        pc++;
        break;
      }
      case OP_TESTF: {
        lua_assert(GET_OPCODE(*pc) == OP_CJMP);
        if (ttype(RB(i)) == LUA_TNIL) {
          setnilvalue(ra);
          dojump(pc, *pc);
        }
        pc++;
        break;
      }
      case OP_NILJMP: {
        setnilvalue(ra);
        pc++;
        break;
      }
      case OP_CALL: {
        int c;
        int b = GETARG_B(i);
        if (b != NO_REG)
          L->top = ra+b+1;
        luaD_call(L, ra);
        c = GETARG_C(i);
        if (c != NO_REG) {
          while (L->top < ra+c) setnilvalue(L->top++);
          L->top = base + tf->maxstacksize;
        }
        break;
      }
      case OP_RETURN: {
        int b;
        luaF_close(L, base);
        b = GETARG_B(i);
        if (b != NO_REG)
          L->top = ra+b;
        return ra;
      }
      case OP_FORPREP: {
        if (luaV_tonumber(ra, ra) == NULL)
          luaD_error(L, "`for' initial value must be a number");
        if (luaV_tonumber(ra+1, ra+1) == NULL)
          luaD_error(L, "`for' limit must be a number");
        if (luaV_tonumber(ra+2, ra+2) == NULL)
          luaD_error(L, "`for' step must be a number");
        /* decrement index (to be incremented) */
        chgnvalue(ra, nvalue(ra) - nvalue(ra+2));
        pc += -GETARG_sBc(i);  /* `jump' to loop end (delta is negated here) */
        /* store in `ra+1' total number of repetitions */
        chgnvalue(ra+1, (nvalue(ra+1)-nvalue(ra))/nvalue(ra+2));
        /* go through */
      }
      case OP_FORLOOP: {
        runtime_check(L, ttype(ra+1) == LUA_TNUMBER &&
                         ttype(ra+2) == LUA_TNUMBER);
        if (ttype(ra) != LUA_TNUMBER)
          luaD_error(L, "`for' index must be a number");
        chgnvalue(ra+1, nvalue(ra+1) - 1);  /* decrement counter */
        if (nvalue(ra+1) >= 0) {
          chgnvalue(ra, nvalue(ra) + nvalue(ra+2));  /* increment index */
          dojump(pc, i);  /* repeat loop */
        }
        break;
      }
      case OP_TFORPREP: {
        if (ttype(ra) != LUA_TTABLE)
          luaD_error(L, "`for' table must be a table");
        setnvalue(ra+1, -1);  /* initial index */
        setnilvalue(ra+2);
        setnilvalue(ra+3);
        pc += -GETARG_sBc(i);  /* `jump' to loop end (delta is negated here) */
        /* go through */
      }
      case OP_TFORLOOP: {
        Table *t;
        int n;
        runtime_check(L, ttype(ra) == LUA_TTABLE &&
                         ttype(ra+1) == LUA_TNUMBER);
        t = hvalue(ra);
        n = cast(int, nvalue(ra+1));
        n = luaH_nexti(t, n, ra+2);
        if (n != -1) {  /* repeat loop? */
          setnvalue(ra+1, n);  /* index */
          dojump(pc, i);  /* repeat loop */
        }
        break;
      }
      case OP_SETLIST:
      case OP_SETLISTO: {
        int bc;
        int n;
        Table *h;
        runtime_check(L, ttype(ra) == LUA_TTABLE);
        h = hvalue(ra);
        bc = GETARG_Bc(i);
        if (GET_OPCODE(i) == OP_SETLIST)
          n = (bc&(LFIELDS_PER_FLUSH-1)) + 1;
        else
          n = L->top - ra - 1;
        bc &= ~(LFIELDS_PER_FLUSH-1);  /* bc = bc - bc%FPF */
        for (; n > 0; n--)
          luaH_setnum(L, h, bc+n, ra+n);
        break;
      }
      case OP_CLOSE: {
        luaF_close(L, ra);
        break;
      }
      case OP_CLOSURE: {
        Proto *p;
        Closure *ncl;
        int nup, j;
        luaV_checkGC(L, L->top);
        p = tf->p[GETARG_Bc(i)];
        nup = p->nupvalues;
        ncl = luaF_newLclosure(L, nup);
        ncl->l.p = p;
        for (j=0; j<nup; j++, pc++) {
          if (GET_OPCODE(*pc) == OP_GETUPVAL)
            ncl->l.upvals[j] = cl->upvals[GETARG_B(*pc)];
          else {
            lua_assert(GET_OPCODE(*pc) == OP_MOVE);
            ncl->l.upvals[j] = luaF_findupval(L, base + GETARG_B(*pc));
          }
        }
        setclvalue(ra, ncl);
        break;
      }
    }
  }
}

