/*
** $Id: lvm.c,v 1.225 2002/04/10 12:11:07 roberto Exp roberto $
** Lua virtual machine
** See Copyright Notice in lua.h
*/


#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"

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


/* limit for table tag-method chains (to avoid loops) */
#define MAXTAGLOOP	10000


static void luaV_checkGC (lua_State *L, StkId top) {
  if (G(L)->nblocks >= G(L)->GCthreshold) {
    L->top = top;  /* limit for active registers */
    luaC_collectgarbage(L);
    L->top = L->ci->top;  /* restore old top position */
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
    return 0;
  else {
    char s[32];  /* 16 digits, sign, point and \0  (+ some extra...) */
    lua_number2str(s, nvalue(obj));
    setsvalue(obj, luaS_new(L, s));
    return 1;
  }
}


static void traceexec (lua_State *L) {
  CallInfo *ci = L->ci;
  Proto *p = ci_func(ci)->l.p;
  int newline = p->lineinfo[pcRel(*ci->pc, p)];
  if (pcRel(*ci->pc, p) == 0)  /* tracing may be starting now? */
    ci->savedpc = *ci->pc;  /* initialize `savedpc' */
  /* calls linehook when enters a new line or jumps back (loop) */
  if (*ci->pc <= ci->savedpc || newline != p->lineinfo[pcRel(ci->savedpc, p)]) {
    luaD_lineHook(L, newline);
    ci = L->ci;  /* previous call may reallocate `ci' */
  }
  ci->savedpc = *ci->pc;
}


static void callTMres (lua_State *L, const TObject *f,
                       const TObject *p1, const TObject *p2, TObject *result ) {
  ptrdiff_t res = savestack(L, result);
  setobj(L->top, f);  /* push function */
  setobj(L->top+1, p1);  /* 1st argument */
  setobj(L->top+2, p2);  /* 2nd argument */
  luaD_checkstack(L, 3);  /* cannot check before (could invalidate p1, p2) */
  L->top += 3;
  luaD_call(L, L->top - 3, 1);
  result = restorestack(L, res);  /* previous call may change stack */
  setobj(result, --L->top);  /* get result */
}



static void callTM (lua_State *L, const TObject *f,
                    const TObject *p1, const TObject *p2, const TObject *p3) {
  setobj(L->top, f);  /* push function */
  setobj(L->top+1, p1);  /* 1st argument */
  setobj(L->top+2, p2);  /* 2nd argument */
  setobj(L->top+3, p3);  /* 3th argument */
  luaD_checkstack(L, 4);  /* cannot check before (could invalidate p1...p3) */
  L->top += 4;
  luaD_call(L, L->top - 4, 0);
}


/*
** Function to index a table.
** Receives the table at `t' and the key at `key'.
** leaves the result at `res'.
*/
void luaV_gettable (lua_State *L, const TObject *t, TObject *key, StkId res) {
  const TObject *tm;
  int loop = 0;
  init:
  if (ttype(t) == LUA_TTABLE) {  /* `t' is a table? */
    Table *et = hvalue(t)->metatable;
    if ((tm = fasttm(L, et, TM_GETTABLE)) == NULL) {  /* no gettable TM? */
      const TObject *h = luaH_get(hvalue(t), key);  /* do a primitive get */
      /* result is no nil or there is no `index' tag method? */
      if (ttype(h) != LUA_TNIL ||  /* no nil? */
          (tm = fasttm(L, et, TM_INDEX)) == NULL) {  /* or no index TM? */
        setobj(res, h);  /* default get */
        return;
      }
    }
    /* else will try the tag method */
  } else {  /* not a table; try a `gettable' tag method */
    if (ttype(tm = luaT_gettmbyobj(L, t, TM_GETTABLE)) == LUA_TNIL) {
      luaG_typeerror(L, t, "index");
      return;  /* to avoid warnings */
    }
  }
  if (ttype(tm) == LUA_TFUNCTION)
    callTMres(L, tm, t, key, res);
  else {
    if (++loop == MAXTAGLOOP) luaD_runerror(L, "loop in gettable");
    t = tm;
    goto init;  /* return luaV_gettable(L, tm, key, res); */
  }
}


/*
** Receives table at `t', key at `key' and value at `val'.
*/
void luaV_settable (lua_State *L, const TObject *t, TObject *key, StkId val) {
  const TObject *tm;
  int loop = 0;
  init:
  if (ttype(t) == LUA_TTABLE) {  /* `t' is a table? */
    Table *et = hvalue(t)->metatable;
    if ((tm = fasttm(L, et, TM_SETTABLE)) == NULL) {  /* no TM? */
      luaH_set(L, hvalue(t), key, val);  /* do a primitive set */
      return;
    }
    /* else will try the tag method */
  } else {  /* not a table; try a `settable' tag method */
    if (ttype(tm = luaT_gettmbyobj(L, t, TM_SETTABLE)) == LUA_TNIL) {
      luaG_typeerror(L, t, "index");
      return;  /* to avoid warnings */
    }
  }
  if (ttype(tm) == LUA_TFUNCTION)
    callTM(L, tm, t, key, val);
  else {
    if (++loop == MAXTAGLOOP) luaD_runerror(L, "loop in settable");
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
  callTMres(L, tm, p1, p2, res);
  return 1;
}


static void call_arith (lua_State *L, StkId p1, const TObject *p2,
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
    return !l_isfalse(L->top);
  }
}


void luaV_strconc (lua_State *L, int total, int last) {
  do {
    StkId top = L->ci->base + last + 1;
    int n = 2;  /* number of elements handled in this pass (at least 2) */
    if (!tostring(L, top-2) || !tostring(L, top-1)) {
      if (!call_binTM(L, top-2, top-1, top-2, TM_CONCAT))
        luaG_concaterror(L, top-2, top-1);
    } else if (tsvalue(top-1)->tsv.len > 0) {  /* if len=0, do nothing */
      /* at least two string values; get as many as possible */
      lu_mem tl = cast(lu_mem, tsvalue(top-1)->tsv.len) +
                  cast(lu_mem, tsvalue(top-2)->tsv.len);
      char *buffer;
      int i;
      while (n < total && tostring(L, top-n-1)) {  /* collect total length */
        tl += tsvalue(top-n-1)->tsv.len;
        n++;
      }
      if (tl > MAX_SIZET) luaD_runerror(L, "string size overflow");
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
    last -= n-1;
  } while (total > 1);  /* repeat until only 1 result left */
}


static void powOp (lua_State *L, StkId ra, StkId rb, StkId rc) {
  const TObject *b = rb;
  const TObject *c = rc;
  TObject tempb, tempc;
  if (tonumber(b, &tempb) && tonumber(c, &tempc)) {
    TObject f, o;
    setsvalue(&o, luaS_newliteral(L, "pow"));
    luaV_gettable(L, gt(L), &o, &f);
    if (ttype(&f) != LUA_TFUNCTION)
      luaD_runerror(L, "`pow' (for `^' operator) is not a function");
    callTMres(L, &f, b, c, ra);
  }
  else
    call_arith(L, rb, rc, ra, TM_POW);
}



/*
** some macros for common tasks in `luaV_execute'
*/

#define runtime_check(L, c)	{ if (!(c)) return 0; }

#define RA(i)	(base+GETARG_A(i))
#define RB(i)	(base+GETARG_B(i))
#define RC(i)	(base+GETARG_C(i))
#define RKC(i)	((GETARG_C(i) < MAXSTACK) ? \
			base+GETARG_C(i) : \
			k+GETARG_C(i)-MAXSTACK)
#define KBc(i)	(k+GETARG_Bc(i))

#define Arith(op, optm)	{ \
  const TObject *b = RB(i); const TObject *c = RKC(i);		\
  TObject tempb, tempc; \
  if (tonumber(b, &tempb) && tonumber(c, &tempc)) { \
    setnvalue(ra, nvalue(b) op nvalue(c));		\
  } else		\
    call_arith(L, RB(i), RKC(i), ra, optm); \
}


#define dojump(pc, i)	((pc) += (i))


StkId luaV_execute (lua_State *L) {
  StkId base;
  LClosure *cl;
  TObject *k;
  const Instruction *pc;
 callentry:  /* entry point when calling new functions */
  L->ci->pc = &pc;
  L->ci->pb = &base;
  pc = L->ci->savedpc;
 retentry:  /* entry point when returning to old functions */
  base = L->ci->base;
  cl = &clvalue(base - 1)->l;
  k = cl->p->k;
  /* main loop of interpreter */
  for (;;) {
    const Instruction i = *pc++;
    StkId ra;
    if (L->linehook)
      traceexec(L);
    ra = RA(i);
    lua_assert(L->top <= L->stack + L->stacksize && L->top >= L->ci->base);
    lua_assert(L->top == L->ci->top ||
         GET_OPCODE(i) == OP_CALL ||   GET_OPCODE(i) == OP_TAILCALL ||
         GET_OPCODE(i) == OP_RETURN || GET_OPCODE(i) == OP_SETLISTO);
    switch (GET_OPCODE(i)) {
      case OP_MOVE: {
        setobj(ra, RB(i));
        break;
      }
      case OP_LOADK: {
        setobj(ra, KBc(i));
        break;
      }
      case OP_LOADBOOL: {
        setbvalue(ra, GETARG_B(i));
        if (GETARG_C(i)) pc++;  /* skip next instruction (if C) */
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
        if (tonumber(rb, ra)) {
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
        int res = l_isfalse(RB(i));  /* next assignment may change this value */
        setbvalue(ra, res);
        break;
      }
      case OP_CONCAT: {
        int b = GETARG_B(i);
        int c = GETARG_C(i);
        luaV_strconc(L, c-b+1, c);  /* may change `base' (and `ra') */
        setobj(base+GETARG_A(i), base+b);
        luaV_checkGC(L, base+c+1);
        break;
      }
      case OP_JMP: {
        dojump(pc, GETARG_sBc(i));
        break;
      }
      case OP_TESTEQ: {  /* skip next instruction if test fails */
        if (!luaO_equalObj(ra, RKC(i))) pc++;
        break;
      }
      case OP_TESTNE: {
        if (luaO_equalObj(ra, RKC(i))) pc++;
        break;
      }
      case OP_TESTLT: {
        if (!luaV_lessthan(L, ra, RKC(i))) pc++;
        break;
      }
      case OP_TESTLE: {  /* b <= c  ===  !(c<b) */
        if (luaV_lessthan(L, RKC(i), ra)) pc++;
        break;
      }
      case OP_TESTGT: {  /* b > c  ===  (c<b) */
        if (!luaV_lessthan(L, RKC(i), ra)) pc++;
        break;
      }
      case OP_TESTGE: {  /* b >= c  === !(b<c) */
        if (luaV_lessthan(L, ra, RKC(i))) pc++;
        break;
      }
      case OP_TESTT: {
        StkId rb = RB(i);
        if (l_isfalse(rb)) pc++;
        else setobj(ra, rb);
        break;
      }
      case OP_TESTF: {
        StkId rb = RB(i);
        if (!l_isfalse(rb)) pc++;
        else setobj(ra, rb);
        break;
      }
      case OP_CALL: {
        StkId firstResult;
        int b = GETARG_B(i);
        int nresults;
        if (b != 0) L->top = ra+b;  /* else previous instruction set top */
        nresults = GETARG_C(i) - 1;
        firstResult = luaD_precall(L, ra);
        if (firstResult) {
          if (firstResult > L->top) {  /* yield? */
            (L->ci-1)->savedpc = pc;
            return NULL;
          }
          /* it was a C function (`precall' called it); adjust results */
          luaD_poscall(L, nresults, firstResult);
          if (nresults >= 0) L->top = L->ci->top;
        }
        else {  /* it is a Lua function: `call' it */
          (L->ci-1)->savedpc = pc;
          goto callentry;
        }
        break;
      }
      case OP_TAILCALL: {
        int b = GETARG_B(i);
        if (L->openupval) luaF_close(L, base);
        if (b != 0) L->top = ra+b;  /* else previous instruction set top */
        luaD_poscall(L, LUA_MULTRET, ra);  /* move down function and args. */
        ra = luaD_precall(L, base-1);
        if (ra == NULL) goto callentry;  /* it is a Lua function */
        else if (ra > L->top) return NULL;  /* yield??? */
        else goto ret;
      }
      case OP_RETURN: {
        int b;
        if (L->openupval) luaF_close(L, base);
        b = GETARG_B(i);
        if (b != 0) L->top = ra+b-1;
        lua_assert(L->ci->pc == &pc);
      }
      ret: {
        CallInfo *ci;
        ci = L->ci - 1;
        if (ci->pc != &pc)  /* previous function was running `here'? */
          return ra;  /* no: return */
        else {  /* yes: continue its execution */
          int nresults;
          lua_assert(ttype(ci->base-1) == LUA_TFUNCTION);
          pc = ci->savedpc;
          lua_assert(GET_OPCODE(*(pc-1)) == OP_CALL);
          nresults = GETARG_C(*(pc-1)) - 1;
          luaD_poscall(L, nresults, ra);
          if (nresults >= 0) L->top = L->ci->top;
          goto retentry;
        }
        break;
      }
      case OP_FORLOOP: {
        lua_Number step, index, limit;
        int j = GETARG_sBc(i);
        const TObject *plimit = ra+1;
        const TObject *pstep = ra+2;
        if (ttype(ra) != LUA_TNUMBER)
          luaD_runerror(L, "`for' initial value must be a number");
        if (!tonumber(plimit, ra+1))
          luaD_runerror(L, "`for' limit must be a number");
        if (!tonumber(pstep, ra+2))
          luaD_runerror(L, "`for' step must be a number");
        step = nvalue(pstep);
        index = nvalue(ra) + step;  /* increment index */
        limit = nvalue(plimit);
        if (step > 0 ? index <= limit : index >= limit) {
          dojump(pc, j);  /* jump back */
          chgnvalue(ra, index);  /* update index */
        }
        break;
      }
      case OP_TFORLOOP: {
        setobj(ra+4, ra+2);
        setobj(ra+3, ra+1);
        setobj(ra+2, ra);
        L->top = ra+5;
        luaD_call(L, ra+2, GETARG_C(i) + 1);
        L->top = L->ci->top;
        if (ttype(ra+2) != LUA_TNIL) pc++;  /* skip jump (keep looping) */
        break;
      }
      case OP_TFORPREP: {
        if (ttype(ra) == LUA_TTABLE) {
          setobj(ra+1, ra);
          setsvalue(ra, luaS_new(L, "next"));
          luaV_gettable(L, gt(L), ra, ra);
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
        else {
          n = L->top - ra - 1;
          L->top = L->ci->top;
        }
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
        p = cl->p->p[GETARG_Bc(i)];
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
        luaV_checkGC(L, L->top);
        break;
      }
    }
  }
}

