/*
** $Id: lvm.c,v 1.197 2001/10/31 19:58:11 roberto Exp $
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
    l_char s[32];  /* 16 digits, sign, point and \0  (+ some extra...) */
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

static StkId callTM (lua_State *L, Closure *f, const l_char *fmt, ...) {
  va_list argp;
  StkId base = L->top;
  lua_assert(strlen(fmt)+1 <= MAXSTACK_TM);
  luaD_checkstack(L, MAXSTACK_TM);
  va_start(argp, fmt);
  setclvalue(L->top, f);  /* push function */
  L->top++;
  while (*fmt) {
    if (*fmt++ == l_c('o')) {
        setobj(L->top, va_arg(argp, TObject *));
    }
    else {
      lua_assert(*(fmt-1) == l_c('s'));
      setsvalue(L->top, va_arg(argp, TString *));
    }
    L->top++;
  }
  luaD_call(L, base);
  va_end(argp);
  return base;
}


#define setTM(L, base)	(L->top = (base))

static void setTMresult (lua_State *L, TObject *result, StkId base) {
  if (L->top == base) {  /* are there valid results? */
    setnilvalue(result);  /* function had no results */
  }
  else {
    setobj(result, base);  /* get first result */
  }
  L->top = base;  /* restore top */
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
  setTMresult(L, res, callTM(L, tm, l_s("oo"), t, key));
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
      luaH_set(L, hvalue(t), key, val);  /* do a primitive set */
      return;
    }
    /* else will call the tag method */
  } else {  /* not a table; try a `settable' tag method */
    tm = luaT_gettmbyObj(G(L), t, TM_SETTABLE);
    if (tm == NULL)  /* no tag method? */
      luaG_typeerror(L, t, l_s("index"));
  }
  setTM(L, callTM(L, tm, l_s("ooo"), t, key, val));
}


void luaV_getglobal (lua_State *L, TString *name, StkId res) {
  const TObject *value = luaH_getstr(hvalue(&L->gt), name);
  Closure *tm;
  if (!HAS_TM_GETGLOBAL(L, ttype(value)) ||  /* is there a tag method? */
      (tm = luaT_gettmbyObj(G(L), value, TM_GETGLOBAL)) == NULL) {
    setobj(res, value);  /* default behavior */
  }
  else
    setTMresult(L, res, callTM(L, tm, l_s("so"), name, value));
}


void luaV_setglobal (lua_State *L, TString *name, StkId val) {
  const TObject *oldvalue = luaH_getstr(hvalue(&L->gt), name);
  Closure *tm;
  if (!HAS_TM_SETGLOBAL(L, ttype(oldvalue)) ||  /* no tag methods? */
     (tm = luaT_gettmbyObj(G(L), oldvalue, TM_SETGLOBAL)) == NULL) {
    if (oldvalue == &luaO_nilobject)
      luaH_setstr(L, hvalue(&L->gt), name, val);  /* raw set */
    else
      settableval(oldvalue, val);  /* warning: tricky optimization! */
  }
  else
    setTM(L, callTM(L, tm, l_s("soo"), name, oldvalue, val));
}


static int call_binTM (lua_State *L, const TObject *p1, const TObject *p2,
                       TObject *res, TMS event) {
  Closure *tm = luaT_gettmbyObj(G(L), p1, event);  /* try first operand */
  if (tm == NULL) {
    tm = luaT_gettmbyObj(G(L), p2, event);  /* try second operand */
    if (tm == NULL) {
      tm = luaT_gettm(G(L), 0, event);  /* try a `global' method */
      if (tm == NULL)
        return 0;  /* no tag method */
    }
  }
  setTMresult(L, res, callTM(L, tm, l_s("oo"), p1, p2));
  return 1;
}


static void call_arith (lua_State *L, StkId p1, TObject *p2,
                        StkId res, TMS event) {
  if (!call_binTM(L, p1, p2, res, event))
    luaG_aritherror(L, p1, p2);
}


static int luaV_strlessthan (const TString *ls, const TString *rs) {
  const l_char *l = getstr(ls);
  size_t ll = ls->tsv.len;
  const l_char *r = getstr(rs);
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
      l_char *buffer;
      int i;
      while (n < total && !tostring(L, top-n-1)) {  /* collect total length */
        tl += tsvalue(top-n-1)->tsv.len;
        n++;
      }
      if (tl > MAX_SIZET) luaD_error(L, l_s("string size overflow"));
      buffer = luaO_openspace(L, tl, l_char);
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
  TObject n;
  for (i=0; firstelem+i<L->top; i++)
    luaH_setnum(L, htab, i+1, firstelem+i);
  /* store counter in field `n' */
  setnvalue(&n, i);
  luaH_setstr(L, htab, luaS_newliteral(L, l_s("n")), &n);
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
        lua_assert(isclosed(cl->upvals[b]) || cl->upvals[b] < base);
        setobj(ra, cl->upvals[b]);
        break;
      }
      case OP_GETGLOBAL: {
        lua_assert(ttype(KBc(i)) == LUA_TSTRING);
        luaV_getglobal(L, tsvalue(KBc(i)), ra);
        break;
      }
      case OP_GETTABLE: {
        luaV_gettable(L, RB(i), RKC(i), ra);
        break;
      }
      case OP_SETGLOBAL: {
        lua_assert(ttype(KBc(i)) == LUA_TSTRING);
        luaV_setglobal(L, tsvalue(KBc(i)), ra);
        break;
      }
      case OP_SETUPVAL: {
        int b = GETARG_B(i);
        lua_assert(isclosed(cl->upvals[b]) || cl->upvals[b] < base);
        setobj(cl->upvals[b], ra);
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
        call_arith(L, RB(i), RKC(i), ra, TM_POW);
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
          luaD_error(L, l_s("`for' initial value must be a number"));
        if (luaV_tonumber(ra+1, ra+1) == NULL)
          luaD_error(L, l_s("`for' limit must be a number"));
        if (luaV_tonumber(ra+2, ra+2) == NULL)
          luaD_error(L, l_s("`for' step must be a number"));
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
          luaD_error(L, l_s("`for' index must be a number"));
        chgnvalue(ra+1, nvalue(ra+1) - 1);  /* decrement counter */
        if (nvalue(ra+1) >= 0) {
          chgnvalue(ra, nvalue(ra) + nvalue(ra+2));  /* increment index */
          dojump(pc, i);  /* repeat loop */
        }
        break;
      }
      case OP_TFORPREP: {
        if (ttype(ra) != LUA_TTABLE)
          luaD_error(L, l_s("`for' table must be a table"));
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
            ncl->l.upvals[j] = base + GETARG_B(*pc);
          }
        }
        luaF_LConlist(L, ncl);
        setclvalue(ra, ncl);
        break;
      }
    }
  }
}

