/*
** $Id: lvm.c,v 1.246 2002/07/08 20:22:08 roberto Exp roberto $
** Lua virtual machine
** See Copyright Notice in lua.h
*/


#include <stdarg.h>
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


/* function to convert a lua_Number to a string */
#ifndef lua_number2str
#include <stdio.h>
#define lua_number2str(s,n)     sprintf((s), LUA_NUMBER_FMT, (n))
#endif


/* limit for table tag-method chains (to avoid loops) */
#define MAXTAGLOOP	100


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
  int mask = L->hookmask;
  if (mask > LUA_MASKLINE) {  /* instruction hook set? */
    if (L->hookcount == 0) {
      luaD_callhook(L, LUA_HOOKCOUNT, -1);
      resethookcount(L);
      return;
    }
  }
  if (mask & LUA_MASKLINE) {
    CallInfo *ci = L->ci;
    Proto *p = ci_func(ci)->l.p;
    int newline = getline(p, pcRel(*ci->u.l.pc, p));
    if (pcRel(*ci->u.l.pc, p) == 0)  /* tracing may be starting now? */
      ci->savedpc = *ci->u.l.pc;  /* initialize `savedpc' */
    /* calls linehook when enters a new line or jumps back (loop) */
    if (*ci->u.l.pc <= ci->savedpc ||
        newline != getline(p, pcRel(ci->savedpc, p))) {
      luaD_callhook(L, LUA_HOOKLINE, newline);
      ci = L->ci;  /* previous call may reallocate `ci' */
    }
    ci->savedpc = *ci->u.l.pc;
  }
}


static void callTMres (lua_State *L, const TObject *f,
                       const TObject *p1, const TObject *p2) {
  setobj(L->top, f);  /* push function */
  setobj(L->top+1, p1);  /* 1st argument */
  setobj(L->top+2, p2);  /* 2nd argument */
  luaD_checkstack(L, 3);  /* cannot check before (could invalidate p1, p2) */
  L->top += 3;
  luaD_call(L, L->top - 3, 1);
  L->top--;  /* result will be in L->top */
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


static const TObject *luaV_index (lua_State *L, const TObject *t,
                                  TObject *key, int loop) {
  const TObject *tm = fasttm(L, hvalue(t)->metatable, TM_INDEX);
  if (tm == NULL) return &luaO_nilobject;  /* no TM */
  if (ttype(tm) == LUA_TFUNCTION) {
    callTMres(L, tm, t, key);
    return L->top;
  }
  else return luaV_gettable(L, tm, key, loop);
}

static const TObject *luaV_getnotable (lua_State *L, const TObject *t,
                                       TObject *key, int loop) {
  const TObject *tm = luaT_gettmbyobj(L, t, TM_GETTABLE);
  if (ttype(tm) == LUA_TNIL)
    luaG_typeerror(L, t, "index");
  if (ttype(tm) == LUA_TFUNCTION) {
    callTMres(L, tm, t, key);
    return L->top;
  }
  else return luaV_gettable(L, tm, key, loop);
}


/*
** Function to index a table.
** Receives the table at `t' and the key at `key'.
** leaves the result at `res'.
*/
const TObject *luaV_gettable (lua_State *L, const TObject *t, TObject *key,
                              int loop) {
  if (loop > MAXTAGLOOP)
    luaG_runerror(L, "loop in gettable");
  if (ttype(t) == LUA_TTABLE) {  /* `t' is a table? */
    Table *h = hvalue(t);
    const TObject *v = luaH_get(h, key);  /* do a primitive get */
    if (ttype(v) != LUA_TNIL) return v;
    else return luaV_index(L, t, key, loop+1);
  }
  else return luaV_getnotable(L, t, key, loop+1);
}


/*
** Receives table at `t', key at `key' and value at `val'.
*/
void luaV_settable (lua_State *L, const TObject *t, TObject *key, StkId val) {
  const TObject *tm;
  int loop = 0;
  do {
    if (ttype(t) == LUA_TTABLE) {  /* `t' is a table? */
      Table *h = hvalue(t);
      TObject *oldval = luaH_set(L, h, key); /* do a primitive set */
      if (ttype(oldval) != LUA_TNIL ||  /* result is no nil? */
          (tm = fasttm(L, h->metatable, TM_NEWINDEX)) == NULL) { /* or no TM? */
        setobj(oldval, val);
        return;
      }
      /* else will try the tag method */
    }
    else if (ttype(tm = luaT_gettmbyobj(L, t, TM_SETTABLE)) == LUA_TNIL)
      luaG_typeerror(L, t, "index");
    if (ttype(tm) == LUA_TFUNCTION) {
      callTM(L, tm, t, key, val);
      return;
    }
    t = tm;  /* else repeat with `tm' */ 
  } while (++loop <= MAXTAGLOOP);
  luaG_runerror(L, "loop in settable");
}


static int call_binTM (lua_State *L, const TObject *p1, const TObject *p2,
                       TObject *res, TMS event) {
  ptrdiff_t result = savestack(L, res);
  const TObject *tm = luaT_gettmbyobj(L, p1, event);  /* try first operand */
  if (ttype(tm) == LUA_TNIL)
    tm = luaT_gettmbyobj(L, p2, event);  /* try second operand */
  if (ttype(tm) != LUA_TFUNCTION) return 0;
  callTMres(L, tm, p1, p2);
  res = restorestack(L, result);  /* previous call may change stack */
  setobj(res, L->top);
  return 1;
}


static int luaV_strcmp (const TString *ls, const TString *rs) {
  const char *l = getstr(ls);
  size_t ll = ls->tsv.len;
  const char *r = getstr(rs);
  size_t lr = rs->tsv.len;
  for (;;) {
    int temp = strcoll(l, r);
    if (temp != 0) return temp;
    else {  /* strings are equal up to a `\0' */
      size_t len = strlen(l);  /* index of first `\0' in both strings */
      if (len == lr)  /* r is finished? */
        return (len == ll) ? 0 : 1;
      else if (len == ll)  /* l is finished? */
        return -1;  /* l is smaller than r (because r is not finished) */
      /* both strings longer than `len'; go on comparing (after the `\0') */
      len++;
      l += len; ll -= len; r += len; lr -= len;
    }
  }
}


int luaV_lessthan (lua_State *L, const TObject *l, const TObject *r) {
  if (ttype(l) != ttype(r))
    return luaG_ordererror(L, l, r);
  else if (ttype(l) == LUA_TNUMBER)
    return nvalue(l) < nvalue(r);
  else if (ttype(l) == LUA_TSTRING)
    return luaV_strcmp(tsvalue(l), tsvalue(r)) < 0;
  else if (call_binTM(L, l, r, L->top, TM_LT))
    return !l_isfalse(L->top);
  return luaG_ordererror(L, l, r);
}


static int luaV_lessequal (lua_State *L, const TObject *l, const TObject *r) {
  if (ttype(l) != ttype(r))
    return luaG_ordererror(L, l, r);
  else if (ttype(l) == LUA_TNUMBER)
    return nvalue(l) <= nvalue(r);
  else if (ttype(l) == LUA_TSTRING)
    return luaV_strcmp(tsvalue(l), tsvalue(r)) <= 0;
  else if (call_binTM(L, l, r, L->top, TM_LE))  /* first try `le' */
    return !l_isfalse(L->top);
  else if (call_binTM(L, r, l, L->top, TM_LT))  /* else try `lt' */
    return l_isfalse(L->top);
  return luaG_ordererror(L, l, r);
}


int luaV_equalval (lua_State *L, const TObject *t1, const TObject *t2) {
  const TObject *tm = NULL;
  lua_assert(ttype(t1) == ttype(t2));
  switch (ttype(t1)) {
    case LUA_TNIL: return 1;
    case LUA_TNUMBER: return nvalue(t1) == nvalue(t2);
    case LUA_TSTRING: return tsvalue(t1) == tsvalue(t2);
    case LUA_TBOOLEAN: return bvalue(t1) == bvalue(t2);  /* true must be 1 !! */
    case LUA_TUDATAVAL: return pvalue(t1) == pvalue(t2);
    case LUA_TFUNCTION: return clvalue(t1) == clvalue(t2);
    case LUA_TUSERDATA:
      if (uvalue(t1) == uvalue(t2)) return 1;
      else if ((tm = fasttm(L, uvalue(t1)->uv.metatable, TM_EQ)) == NULL &&
               (tm = fasttm(L, uvalue(t2)->uv.metatable, TM_EQ)) == NULL)
        return 0;  /* no TM */
      else break;  /* will try TM */
    case LUA_TTABLE:
      if (hvalue(t1) == hvalue(t2)) return 1;
      else if ((tm = fasttm(L, hvalue(t1)->metatable, TM_EQ)) == NULL &&
               (tm = fasttm(L, hvalue(t2)->metatable, TM_EQ)) == NULL)
        return 0;  /* no TM */
      else break;  /* will try TM */
  }
  callTMres(L, tm, t1, t2);  /* call TM */
  return !l_isfalse(L->top);
}


void luaV_concat (lua_State *L, int total, int last) {
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
      if (tl > MAX_SIZET) luaG_runerror(L, "string size overflow");
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


static void Arith (lua_State *L, StkId ra, StkId rb, StkId rc, TMS op) {
  TObject tempb, tempc;
  const TObject *b, *c;
  if ((b = luaV_tonumber(rb, &tempb)) != NULL &&
      (c = luaV_tonumber(rc, &tempc)) != NULL) {
    switch (op) {
      case TM_ADD: setnvalue(ra, nvalue(b) + nvalue(c)); break;
      case TM_SUB: setnvalue(ra, nvalue(b) - nvalue(c)); break;
      case TM_MUL: setnvalue(ra, nvalue(b) * nvalue(c)); break;
      case TM_DIV: setnvalue(ra, nvalue(b) / nvalue(c)); break;
      case TM_POW: {
        const TObject *f = luaH_getstr(hvalue(registry(L)),
                                       G(L)->tmname[TM_POW]);
        ptrdiff_t res = savestack(L, ra);
        if (ttype(f) != LUA_TFUNCTION)
          luaG_runerror(L, "`pow' (for `^' operator) is not a function");
        callTMres(L, f, b, c);
        ra = restorestack(L, res);  /* previous call may change stack */
        setobj(ra, L->top);
        break;
      }
      default: lua_assert(0); break;
    }
  }
  else if (!call_binTM(L, rb, rc, ra, op))
    luaG_aritherror(L, rb, rc);
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
#define KBx(i)	(k+GETARG_Bx(i))


#define dojump(pc, i)	((pc) += (i))


StkId luaV_execute (lua_State *L) {
  StkId base;
  LClosure *cl;
  TObject *k;
  const Instruction *pc;
 callentry:  /* entry point when calling new functions */
  L->ci->u.l.pc = &pc;
  L->ci->u.l.pb = &base;
  pc = L->ci->savedpc;
  if (L->hookmask & LUA_MASKCALL)
    luaD_callhook(L, LUA_HOOKCALL, -1);
 retentry:  /* entry point when returning to old functions */
  base = L->ci->base;
  cl = &clvalue(base - 1)->l;
  k = cl->p->k;
  /* main loop of interpreter */
  for (;;) {
    const Instruction i = *pc++;
    StkId ra;
    if (L->hookmask >= LUA_MASKLINE &&
        (--L->hookcount == 0 || L->hookmask & LUA_MASKLINE))
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
        setobj(ra, KBx(i));
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
        StkId rb = KBx(i);
        const TObject *v;
        lua_assert(ttype(rb) == LUA_TSTRING && ttype(&cl->g) == LUA_TTABLE);
        v = luaH_getstr(hvalue(&cl->g), tsvalue(rb));
        if (ttype(v) != LUA_TNIL) { setobj(ra, v); }
        else
          setobj(RA(i), luaV_index(L, &cl->g, rb, 0));
        break;
      }
      case OP_GETTABLE: {
        StkId rb = RB(i);
        TObject *rc = RKC(i);
        if (ttype(rb) == LUA_TTABLE) {
          const TObject *v = luaH_get(hvalue(rb), rc);
          if (ttype(v) != LUA_TNIL) { setobj(ra, v); }
          else
            setobj(RA(i), luaV_index(L, rb, rc, 0));
        }
        else
          setobj(RA(i), luaV_getnotable(L, rb, rc, 0));
        break;
      }
      case OP_SETGLOBAL: {
        lua_assert(ttype(KBx(i)) == LUA_TSTRING && ttype(&cl->g) == LUA_TTABLE);
        luaV_settable(L, &cl->g, KBx(i), ra);
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
        TObject *rc = RKC(i);
        runtime_check(L, ttype(rc) == LUA_TSTRING);
        setobj(ra+1, rb);
        if (ttype(rb) == LUA_TTABLE) {
          const TObject *v = luaH_getstr(hvalue(rb), tsvalue(rc));
          if (ttype(v) != LUA_TNIL) { setobj(ra, v); }
          else
            setobj(RA(i), luaV_index(L, rb, rc, 0));
        }
        else
          setobj(RA(i), luaV_getnotable(L, rb, rc, 0));
        break;
      }
      case OP_ADD: {
        StkId rb = RB(i);
        StkId rc = RKC(i);
        if (ttype(rb) == LUA_TNUMBER && ttype(rc) == LUA_TNUMBER) {
          setnvalue(ra, nvalue(rb) + nvalue(rc));
        }
        else
          Arith(L, ra, rb, rc, TM_ADD);
        break;
      }
      case OP_SUB: {
        StkId rb = RB(i);
        StkId rc = RKC(i);
        if (ttype(rb) == LUA_TNUMBER && ttype(rc) == LUA_TNUMBER) {
          setnvalue(ra, nvalue(rb) - nvalue(rc));
        }
        else
          Arith(L, ra, rb, rc, TM_SUB);
        break;
      }
      case OP_MUL: {
        StkId rb = RB(i);
        StkId rc = RKC(i);
        if (ttype(rb) == LUA_TNUMBER && ttype(rc) == LUA_TNUMBER) {
          setnvalue(ra, nvalue(rb) * nvalue(rc));
        }
        else
          Arith(L, ra, rb, rc, TM_MUL);
        break;
      }
      case OP_DIV: {
        StkId rb = RB(i);
        StkId rc = RKC(i);
        if (ttype(rb) == LUA_TNUMBER && ttype(rc) == LUA_TNUMBER) {
          setnvalue(ra, nvalue(rb) / nvalue(rc));
        }
        else
          Arith(L, ra, rb, rc, TM_DIV);
        break;
      }
      case OP_POW: {
        Arith(L, ra, RB(i), RKC(i), TM_POW);
        break;
      }
      case OP_UNM: {
        const TObject *rb = RB(i);
        TObject temp;
        if (tonumber(rb, &temp)) {
          setnvalue(ra, -nvalue(rb));
        }
        else {
          setnilvalue(&temp);
          if (!call_binTM(L, RB(i), &temp, ra, TM_UNM))
            luaG_aritherror(L, RB(i), &temp);
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
        luaV_concat(L, c-b+1, c);  /* may change `base' (and `ra') */
        setobj(base+GETARG_A(i), base+b);
        luaV_checkGC(L, base+c+1);
        break;
      }
      case OP_JMP: {
        dojump(pc, GETARG_sBx(i));
        break;
      }
      case OP_EQ: {  /* skip next instruction if test fails */
        if (equalobj(L, ra, RKC(i)) != GETARG_B(i)) pc++;
        else dojump(pc, GETARG_sBx(*pc) + 1);
        break;
      }
      case OP_LT: {
        if (luaV_lessthan(L, ra, RKC(i)) != GETARG_B(i)) pc++;
        else dojump(pc, GETARG_sBx(*pc) + 1);
        break;
      }
      case OP_LE: {
        if (luaV_lessequal(L, ra, RKC(i)) != GETARG_B(i)) pc++;
        else dojump(pc, GETARG_sBx(*pc) + 1);
        break;
      }
      case OP_GT: {
        if (luaV_lessthan(L, RKC(i), ra) != GETARG_B(i)) pc++;
        else dojump(pc, GETARG_sBx(*pc) + 1);
        break;
      }
      case OP_GE: {
        if (luaV_lessequal(L, RKC(i), ra) != GETARG_B(i)) pc++;
        else dojump(pc, GETARG_sBx(*pc) + 1);
        break;
      }
      case OP_TEST: {
        StkId rc = RKC(i);
        if (l_isfalse(rc) == GETARG_B(i)) pc++;
        else {
          setobj(ra, rc);
          dojump(pc, GETARG_sBx(*pc) + 1);
        }
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
        lua_assert(L->ci->u.l.pc == &pc);
      }
      ret: {
        CallInfo *ci;
        ci = L->ci - 1;
        /* previous function was running `here'? */
        if (!isLua(ci) || ci->u.l.pc != &pc)
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
      }
      case OP_FORLOOP: {
        lua_Number step, index, limit;
        const TObject *plimit = ra+1;
        const TObject *pstep = ra+2;
        if (ttype(ra) != LUA_TNUMBER)
          luaG_runerror(L, "`for' initial value must be a number");
        if (!tonumber(plimit, ra+1))
          luaG_runerror(L, "`for' limit must be a number");
        if (!tonumber(pstep, ra+2))
          luaG_runerror(L, "`for' step must be a number");
        step = nvalue(pstep);
        index = nvalue(ra) + step;  /* increment index */
        limit = nvalue(plimit);
        if (step > 0 ? index <= limit : index >= limit) {
          dojump(pc, GETARG_sBx(i));  /* jump back */
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
        if (ttype(ra+2) == LUA_TNIL) pc++;  /* skip jump (break loop) */
        else dojump(pc, GETARG_sBx(*pc) + 1);  /* else jump back */
        break;
      }
      case OP_TFORPREP: {  /* for compatibility only */
        if (ttype(ra) == LUA_TTABLE) {
          setobj(ra+1, ra);
          setobj(ra, luaH_getstr(hvalue(gt(L)), luaS_new(L, "next")));
        }
        dojump(pc, GETARG_sBx(i));
        break;
      }
      case OP_SETLIST:
      case OP_SETLISTO: {
        int bc;
        int n;
        Table *h;
        runtime_check(L, ttype(ra) == LUA_TTABLE);
        h = hvalue(ra);
        bc = GETARG_Bx(i);
        if (GET_OPCODE(i) == OP_SETLIST)
          n = (bc&(LFIELDS_PER_FLUSH-1)) + 1;
        else {
          n = L->top - ra - 1;
          L->top = L->ci->top;
        }
        bc &= ~(LFIELDS_PER_FLUSH-1);  /* bc = bc - bc%FPF */
        for (; n > 0; n--)
          setobj(luaH_setnum(L, h, bc+n), ra+n);
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
        p = cl->p->p[GETARG_Bx(i)];
        nup = p->nupvalues;
        ncl = luaF_newLclosure(L, nup, &cl->g);
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

