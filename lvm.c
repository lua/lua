/*
** $Id: lvm.c,v 2.358 2018/06/15 14:14:20 roberto Exp roberto $
** Lua virtual machine
** See Copyright Notice in lua.h
*/

#define lvm_c
#define LUA_CORE

#include "lprefix.h"

#include <float.h>
#include <limits.h>
#include <math.h>
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


/*
** By default, use jump tables in the main interpreter loop on gcc
** and compatible compilers.
*/
#if !defined(LUA_USE_JUMPTABLE)
#if defined(__GNUC__)
#define LUA_USE_JUMPTABLE	1
#else
#define LUA_USE_JUMPTABLE	0
#endif
#endif



/* limit for table tag-method chains (to avoid infinite loops) */
#define MAXTAGLOOP	2000


/*
** 'l_intfitsf' checks whether a given integer is in the range that
** can be converted to a float without rounding. Used in comparisons.
*/

/* number of bits in the mantissa of a float */
#define NBM		(l_mathlim(MANT_DIG))

/*
** Check whether some integers may not fit in a float, testing whether
** (maxinteger >> NBM) > 0. (That implies (1 << NBM) <= maxinteger.)
** (The shifts are done in parts, to avoid shifting by more than the size
** of an integer. In a worst case, NBM == 113 for long double and
** sizeof(long) == 32.)
*/
#if ((((LUA_MAXINTEGER >> (NBM / 4)) >> (NBM / 4)) >> (NBM / 4)) \
	>> (NBM - (3 * (NBM / 4))))  >  0

/* limit for integers that fit in a float */
#define MAXINTFITSF	((lua_Unsigned)1 << NBM)

/* check whether 'i' is in the interval [-MAXINTFITSF, MAXINTFITSF] */
#define l_intfitsf(i)	((MAXINTFITSF + l_castS2U(i)) <= (2 * MAXINTFITSF))

#else  /* all integers fit in a float precisely */

#define l_intfitsf(i)	1

#endif



/*
** Try to convert a value to a float. The float case is already handled
** by the macro 'tonumber'.
*/
int luaV_tonumber_ (const TValue *obj, lua_Number *n) {
  TValue v;
  if (ttisinteger(obj)) {
    *n = cast_num(ivalue(obj));
    return 1;
  }
  else if (cvt2num(obj) &&  /* string coercible to number? */
            luaO_str2num(svalue(obj), &v) == vslen(obj) + 1) {
    *n = nvalue(&v);  /* convert result of 'luaO_str2num' to a float */
    return 1;
  }
  else
    return 0;  /* conversion failed */
}


/*
** try to convert a float to an integer, rounding according to 'mode':
** mode == 0: accepts only integral values
** mode == 1: takes the floor of the number
** mode == 2: takes the ceil of the number
*/
int luaV_flttointeger (lua_Number n, lua_Integer *p, int mode) {
  lua_Number f = l_floor(n);
  if (n != f) {  /* not an integral value? */
    if (mode == 0) return 0;  /* fails if mode demands integral value */
    else if (mode > 1)  /* needs ceil? */
      f += 1;  /* convert floor to ceil (remember: n != f) */
  }
  return lua_numbertointeger(f, p);
}


/*
** try to convert a value to an integer, rounding according to 'mode',
** without string coercion.
** ("Fast track" handled by macro 'tointegerns'.)
*/
int luaV_tointegerns (const TValue *obj, lua_Integer *p, int mode) {
  if (ttisfloat(obj))
    return luaV_flttointeger(fltvalue(obj), p, mode);
  else if (ttisinteger(obj)) {
    *p = ivalue(obj);
    return 1;
  }
  else
    return 0;
}


/*
** try to convert a value to an integer.
*/
int luaV_tointeger (const TValue *obj, lua_Integer *p, int mode) {
  TValue v;
  if (cvt2num(obj) && luaO_str2num(svalue(obj), &v) == vslen(obj) + 1)
    obj = &v;  /* change string to its corresponding number */
  return luaV_tointegerns(obj, p, mode);
}


/*
** Try to convert a 'for' limit to an integer, preserving the semantics
** of the loop.  (The following explanation assumes a non-negative step;
** it is valid for negative steps mutatis mutandis.)
** If the limit is an integer or can be converted to an integer,
** rounding down, that is it.
** Otherwise, check whether the limit can be converted to a float.  If
** the number is too large, it is OK to set the limit as LUA_MAXINTEGER,
** which means no limit.  If the number is too negative, the loop
** should not run, because any initial integer value is larger than the
** limit. So, it sets the limit to LUA_MININTEGER. 'stopnow' corrects
** the extreme case when the initial value is LUA_MININTEGER, in which
** case the LUA_MININTEGER limit would still run the loop once.
*/
static int forlimit (const TValue *obj, lua_Integer *p, lua_Integer step,
                     int *stopnow) {
  *stopnow = 0;  /* usually, let loops run */
  if (ttisinteger(obj))
    *p = ivalue(obj);
  else if (!luaV_tointeger(obj, p, (step < 0 ? 2 : 1))) {
    /* not coercible to in integer */
    lua_Number n;  /* try to convert to float */
    if (!tonumber(obj, &n)) /* cannot convert to float? */
      return 0;  /* not a number */
    if (luai_numlt(0, n)) {  /* if true, float is larger than max integer */
      *p = LUA_MAXINTEGER;
      if (step < 0) *stopnow = 1;
    }
    else {  /* float is less than min integer */
      *p = LUA_MININTEGER;
      if (step >= 0) *stopnow = 1;
    }
  }
  return 1;
}


/*
** Finish the table access 'val = t[key]'.
** if 'slot' is NULL, 't' is not a table; otherwise, 'slot' points to
** t[k] entry (which must be empty).
*/
void luaV_finishget (lua_State *L, const TValue *t, TValue *key, StkId val,
                      const TValue *slot) {
  int loop;  /* counter to avoid infinite loops */
  const TValue *tm;  /* metamethod */
  for (loop = 0; loop < MAXTAGLOOP; loop++) {
    if (slot == NULL) {  /* 't' is not a table? */
      lua_assert(!ttistable(t));
      tm = luaT_gettmbyobj(L, t, TM_INDEX);
      if (unlikely(notm(tm)))
        luaG_typeerror(L, t, "index");  /* no metamethod */
      /* else will try the metamethod */
    }
    else {  /* 't' is a table */
      lua_assert(isempty(slot));
      tm = fasttm(L, hvalue(t)->metatable, TM_INDEX);  /* table's metamethod */
      if (tm == NULL) {  /* no metamethod? */
        setnilvalue(s2v(val));  /* result is nil */
        return;
      }
      /* else will try the metamethod */
    }
    if (ttisfunction(tm)) {  /* is metamethod a function? */
      luaT_callTMres(L, tm, t, key, val);  /* call it */
      return;
    }
    t = tm;  /* else try to access 'tm[key]' */
    if (luaV_fastget(L, t, key, slot, luaH_get)) {  /* fast track? */
      setobj2s(L, val, slot);  /* done */
      return;
    }
    /* else repeat (tail call 'luaV_finishget') */
  }
  luaG_runerror(L, "'__index' chain too long; possible loop");
}


/*
** Finish a table assignment 't[key] = val'.
** If 'slot' is NULL, 't' is not a table.  Otherwise, 'slot' points
** to the entry 't[key]', or to a value with an absent key if there
** is no such entry.  (The value at 'slot' must be empty, otherwise
** 'luaV_fastget' would have done the job.)
*/
void luaV_finishset (lua_State *L, const TValue *t, TValue *key,
                     TValue *val, const TValue *slot) {
  int loop;  /* counter to avoid infinite loops */
  for (loop = 0; loop < MAXTAGLOOP; loop++) {
    const TValue *tm;  /* '__newindex' metamethod */
    if (slot != NULL) {  /* is 't' a table? */
      Table *h = hvalue(t);  /* save 't' table */
      lua_assert(isempty(slot));  /* slot must be empty */
      tm = fasttm(L, h->metatable, TM_NEWINDEX);  /* get metamethod */
      if (tm == NULL) {  /* no metamethod? */
        if (isabstkey(slot))  /* no previous entry? */
          slot = luaH_newkey(L, h, key);  /* create one */
        /* no metamethod and (now) there is an entry with given key */
        setobj2t(L, cast(TValue *, slot), val);  /* set its new value */
        invalidateTMcache(h);
        luaC_barrierback(L, obj2gco(h), val);
        return;
      }
      /* else will try the metamethod */
    }
    else {  /* not a table; check metamethod */
      tm = luaT_gettmbyobj(L, t, TM_NEWINDEX);
      if (unlikely(notm(tm)))
        luaG_typeerror(L, t, "index");
    }
    /* try the metamethod */
    if (ttisfunction(tm)) {
      luaT_callTM(L, tm, t, key, val);
      return;
    }
    t = tm;  /* else repeat assignment over 'tm' */
    if (luaV_fastget(L, t, key, slot, luaH_get)) {
      luaV_finishfastset(L, t, slot, val);
      return;  /* done */
    }
    /* else 'return luaV_finishset(L, t, key, val, slot)' (loop) */
  }
  luaG_runerror(L, "'__newindex' chain too long; possible loop");
}


/*
** Compare two strings 'ls' x 'rs', returning an integer less-equal-
** -greater than zero if 'ls' is less-equal-greater than 'rs'.
** The code is a little tricky because it allows '\0' in the strings
** and it uses 'strcoll' (to respect locales) for each segments
** of the strings.
*/
static int l_strcmp (const TString *ls, const TString *rs) {
  const char *l = getstr(ls);
  size_t ll = tsslen(ls);
  const char *r = getstr(rs);
  size_t lr = tsslen(rs);
  for (;;) {  /* for each segment */
    int temp = strcoll(l, r);
    if (temp != 0)  /* not equal? */
      return temp;  /* done */
    else {  /* strings are equal up to a '\0' */
      size_t len = strlen(l);  /* index of first '\0' in both strings */
      if (len == lr)  /* 'rs' is finished? */
        return (len == ll) ? 0 : 1;  /* check 'ls' */
      else if (len == ll)  /* 'ls' is finished? */
        return -1;  /* 'ls' is less than 'rs' ('rs' is not finished) */
      /* both strings longer than 'len'; go on comparing after the '\0' */
      len++;
      l += len; ll -= len; r += len; lr -= len;
    }
  }
}


/*
** Check whether integer 'i' is less than float 'f'. If 'i' has an
** exact representation as a float ('l_intfitsf'), compare numbers as
** floats. Otherwise, use the equivalence 'i < f <=> i < ceil(f)'.
** If 'ceil(f)' is out of integer range, either 'f' is greater than
** all integers or less than all integers.
** (The test with 'l_intfitsf' is only for performance; the else
** case is correct for all values, but it is slow due to the conversion
** from float to int.)
** When 'f' is NaN, comparisons must result in false.
*/
static int LTintfloat (lua_Integer i, lua_Number f) {
  if (l_intfitsf(i))
    return luai_numlt(cast_num(i), f);  /* compare them as floats */
  else {  /* i < f <=> i < ceil(f) */
    lua_Integer fi;
    if (luaV_flttointeger(f, &fi, 2))  /* fi = ceil(f) */
      return i < fi;   /* compare them as integers */
    else  /* 'f' is either greater or less than all integers */
      return f > 0;  /* greater? */
  }
}


/*
** Check whether integer 'i' is less than or equal to float 'f'.
** See comments on previous function.
*/
static int LEintfloat (lua_Integer i, lua_Number f) {
  if (l_intfitsf(i))
    return luai_numle(cast_num(i), f);  /* compare them as floats */
  else {  /* i <= f <=> i <= floor(f) */
    lua_Integer fi;
    if (luaV_flttointeger(f, &fi, 1))  /* fi = floor(f) */
      return i <= fi;   /* compare them as integers */
    else  /* 'f' is either greater or less than all integers */
      return f > 0;  /* greater? */
  }
}


/*
** Check whether float 'f' is less than integer 'i'.
** See comments on previous function.
*/
static int LTfloatint (lua_Number f, lua_Integer i) {
  if (l_intfitsf(i))
    return luai_numlt(f, cast_num(i));  /* compare them as floats */
  else {  /* f < i <=> floor(f) < i */
    lua_Integer fi;
    if (luaV_flttointeger(f, &fi, 1))  /* fi = floor(f) */
      return fi < i;   /* compare them as integers */
    else  /* 'f' is either greater or less than all integers */
      return f < 0;  /* less? */
  }
}


/*
** Check whether float 'f' is less than or equal to integer 'i'.
** See comments on previous function.
*/
static int LEfloatint (lua_Number f, lua_Integer i) {
  if (l_intfitsf(i))
    return luai_numle(f, cast_num(i));  /* compare them as floats */
  else {  /* f <= i <=> ceil(f) <= i */
    lua_Integer fi;
    if (luaV_flttointeger(f, &fi, 2))  /* fi = ceil(f) */
      return fi <= i;   /* compare them as integers */
    else  /* 'f' is either greater or less than all integers */
      return f < 0;  /* less? */
  }
}


/*
** Return 'l < r', for numbers.
*/
static int LTnum (const TValue *l, const TValue *r) {
  lua_assert(ttisnumber(l) && ttisnumber(r));
  if (ttisinteger(l)) {
    lua_Integer li = ivalue(l);
    if (ttisinteger(r))
      return li < ivalue(r);  /* both are integers */
    else  /* 'l' is int and 'r' is float */
      return LTintfloat(li, fltvalue(r));  /* l < r ? */
  }
  else {
    lua_Number lf = fltvalue(l);  /* 'l' must be float */
    if (ttisfloat(r))
      return luai_numlt(lf, fltvalue(r));  /* both are float */
    else  /* 'l' is float and 'r' is int */
      return LTfloatint(lf, ivalue(r));
  }
}


/*
** Return 'l <= r', for numbers.
*/
static int LEnum (const TValue *l, const TValue *r) {
  lua_assert(ttisnumber(l) && ttisnumber(r));
  if (ttisinteger(l)) {
    lua_Integer li = ivalue(l);
    if (ttisinteger(r))
      return li <= ivalue(r);  /* both are integers */
    else  /* 'l' is int and 'r' is float */
      return LEintfloat(li, fltvalue(r));  /* l <= r ? */
  }
  else {
    lua_Number lf = fltvalue(l);  /* 'l' must be float */
    if (ttisfloat(r))
      return luai_numle(lf, fltvalue(r));  /* both are float */
    else  /* 'l' is float and 'r' is int */
      return LEfloatint(lf, ivalue(r));
  }
}


/*
** return 'l < r' for non-numbers.
*/
static int lessthanothers (lua_State *L, const TValue *l, const TValue *r) {
  lua_assert(!ttisnumber(l) || !ttisnumber(r));
  if (ttisstring(l) && ttisstring(r))  /* both are strings? */
    return l_strcmp(tsvalue(l), tsvalue(r)) < 0;
  else
    return luaT_callorderTM(L, l, r, TM_LT);
}


/*
** Main operation less than; return 'l < r'.
*/
int luaV_lessthan (lua_State *L, const TValue *l, const TValue *r) {
  if (ttisnumber(l) && ttisnumber(r))  /* both operands are numbers? */
    return LTnum(l, r);
  else return lessthanothers(L, l, r);
}


/*
** return 'l <= r' for non-numbers.
** If it needs a metamethod and there is no '__le', try '__lt', based
** on l <= r iff !(r < l) (assuming a total order). If the metamethod
** yields during this substitution, the continuation has to know about
** it (to negate the result of r<l); bit CIST_LEQ in the call status
** keeps that information.
*/
static int lessequalothers (lua_State *L, const TValue *l, const TValue *r) {
  lua_assert(!ttisnumber(l) || !ttisnumber(r));
  if (ttisstring(l) && ttisstring(r))  /* both are strings? */
    return l_strcmp(tsvalue(l), tsvalue(r)) <= 0;
  else
    return luaT_callorderTM(L, l, r, TM_LE);
}


/*
** Main operation less than or equal to; return 'l <= r'.
*/
int luaV_lessequal (lua_State *L, const TValue *l, const TValue *r) {
  if (ttisnumber(l) && ttisnumber(r))  /* both operands are numbers? */
    return LEnum(l, r);
  else return lessequalothers(L, l, r);
}


/*
** Main operation for equality of Lua values; return 't1 == t2'.
** L == NULL means raw equality (no metamethods)
*/
int luaV_equalobj (lua_State *L, const TValue *t1, const TValue *t2) {
  const TValue *tm;
  if (ttypetag(t1) != ttypetag(t2)) {  /* not the same variant? */
    if (ttype(t1) != ttype(t2) || ttype(t1) != LUA_TNUMBER)
      return 0;  /* only numbers can be equal with different variants */
    else {  /* two numbers with different variants */
      lua_Integer i1, i2;  /* compare them as integers */
      return (tointegerns(t1, &i1) && tointegerns(t2, &i2) && i1 == i2);
    }
  }
  /* values have same type and same variant */
  switch (ttypetag(t1)) {
    case LUA_TNIL: return 1;
    case LUA_TNUMINT: return (ivalue(t1) == ivalue(t2));
    case LUA_TNUMFLT: return luai_numeq(fltvalue(t1), fltvalue(t2));
    case LUA_TBOOLEAN: return bvalue(t1) == bvalue(t2);  /* true must be 1! */
    case LUA_TLIGHTUSERDATA: return pvalue(t1) == pvalue(t2);
    case LUA_TLCF: return fvalue(t1) == fvalue(t2);
    case LUA_TSHRSTR: return eqshrstr(tsvalue(t1), tsvalue(t2));
    case LUA_TLNGSTR: return luaS_eqlngstr(tsvalue(t1), tsvalue(t2));
    case LUA_TUSERDATA: {
      if (uvalue(t1) == uvalue(t2)) return 1;
      else if (L == NULL) return 0;
      tm = fasttm(L, uvalue(t1)->metatable, TM_EQ);
      if (tm == NULL)
        tm = fasttm(L, uvalue(t2)->metatable, TM_EQ);
      break;  /* will try TM */
    }
    case LUA_TTABLE: {
      if (hvalue(t1) == hvalue(t2)) return 1;
      else if (L == NULL) return 0;
      tm = fasttm(L, hvalue(t1)->metatable, TM_EQ);
      if (tm == NULL)
        tm = fasttm(L, hvalue(t2)->metatable, TM_EQ);
      break;  /* will try TM */
    }
    default:
      return gcvalue(t1) == gcvalue(t2);
  }
  if (tm == NULL)  /* no TM? */
    return 0;  /* objects are different */
  luaT_callTMres(L, tm, t1, t2, L->top);  /* call TM */
  return !l_isfalse(s2v(L->top));
}


/* macro used by 'luaV_concat' to ensure that element at 'o' is a string */
#define tostring(L,o)  \
	(ttisstring(o) || (cvt2str(o) && (luaO_tostring(L, o), 1)))

#define isemptystr(o)	(ttisshrstring(o) && tsvalue(o)->shrlen == 0)

/* copy strings in stack from top - n up to top - 1 to buffer */
static void copy2buff (StkId top, int n, char *buff) {
  size_t tl = 0;  /* size already copied */
  do {
    size_t l = vslen(s2v(top - n));  /* length of string being copied */
    memcpy(buff + tl, svalue(s2v(top - n)), l * sizeof(char));
    tl += l;
  } while (--n > 0);
}


/*
** Main operation for concatenation: concat 'total' values in the stack,
** from 'L->top - total' up to 'L->top - 1'.
*/
void luaV_concat (lua_State *L, int total) {
  lua_assert(total >= 2);
  do {
    StkId top = L->top;
    int n = 2;  /* number of elements handled in this pass (at least 2) */
    if (!(ttisstring(s2v(top - 2)) || cvt2str(s2v(top - 2))) ||
        !tostring(L, s2v(top - 1)))
      luaT_trybinTM(L, s2v(top - 2), s2v(top - 1), top - 2, TM_CONCAT);
    else if (isemptystr(s2v(top - 1)))  /* second operand is empty? */
      cast_void(tostring(L, s2v(top - 2)));  /* result is first operand */
    else if (isemptystr(s2v(top - 2))) {  /* first operand is empty string? */
      setobjs2s(L, top - 2, top - 1);  /* result is second op. */
    }
    else {
      /* at least two non-empty string values; get as many as possible */
      size_t tl = vslen(s2v(top - 1));
      TString *ts;
      /* collect total length and number of strings */
      for (n = 1; n < total && tostring(L, s2v(top - n - 1)); n++) {
        size_t l = vslen(s2v(top - n - 1));
        if (unlikely(l >= (MAX_SIZE/sizeof(char)) - tl))
          luaG_runerror(L, "string length overflow");
        tl += l;
      }
      if (tl <= LUAI_MAXSHORTLEN) {  /* is result a short string? */
        char buff[LUAI_MAXSHORTLEN];
        copy2buff(top, n, buff);  /* copy strings to buffer */
        ts = luaS_newlstr(L, buff, tl);
      }
      else {  /* long string; copy strings directly to final result */
        ts = luaS_createlngstrobj(L, tl);
        copy2buff(top, n, getstr(ts));
      }
      setsvalue2s(L, top - n, ts);  /* create result */
    }
    total -= n-1;  /* got 'n' strings to create 1 new */
    L->top -= n-1;  /* popped 'n' strings and pushed one */
  } while (total > 1);  /* repeat until only 1 result left */
}


/*
** Main operation 'ra' = #rb'.
*/
void luaV_objlen (lua_State *L, StkId ra, const TValue *rb) {
  const TValue *tm;
  switch (ttypetag(rb)) {
    case LUA_TTABLE: {
      Table *h = hvalue(rb);
      tm = fasttm(L, h->metatable, TM_LEN);
      if (tm) break;  /* metamethod? break switch to call it */
      setivalue(s2v(ra), luaH_getn(h));  /* else primitive len */
      return;
    }
    case LUA_TSHRSTR: {
      setivalue(s2v(ra), tsvalue(rb)->shrlen);
      return;
    }
    case LUA_TLNGSTR: {
      setivalue(s2v(ra), tsvalue(rb)->u.lnglen);
      return;
    }
    default: {  /* try metamethod */
      tm = luaT_gettmbyobj(L, rb, TM_LEN);
      if (unlikely(notm(tm)))  /* no metamethod? */
        luaG_typeerror(L, rb, "get length of");
      break;
    }
  }
  luaT_callTMres(L, tm, rb, rb, ra);
}


/*
** Integer division; return 'm // n', that is, floor(m/n).
** C division truncates its result (rounds towards zero).
** 'floor(q) == trunc(q)' when 'q >= 0' or when 'q' is integer,
** otherwise 'floor(q) == trunc(q) - 1'.
*/
lua_Integer luaV_div (lua_State *L, lua_Integer m, lua_Integer n) {
  if (l_castS2U(n) + 1u <= 1u) {  /* special cases: -1 or 0 */
    if (unlikely(n == 0))
      luaG_runerror(L, "attempt to divide by zero");
    return intop(-, 0, m);   /* n==-1; avoid overflow with 0x80000...//-1 */
  }
  else {
    lua_Integer q = m / n;  /* perform C division */
    if ((m ^ n) < 0 && m % n != 0)  /* 'm/n' would be negative non-integer? */
      q -= 1;  /* correct result for different rounding */
    return q;
  }
}


/*
** Integer modulus; return 'm % n'. (Assume that C '%' with
** negative operands follows C99 behavior. See previous comment
** about luaV_div.)
*/
lua_Integer luaV_mod (lua_State *L, lua_Integer m, lua_Integer n) {
  if (l_castS2U(n) + 1u <= 1u) {  /* special cases: -1 or 0 */
    if (unlikely(n == 0))
      luaG_runerror(L, "attempt to perform 'n%%0'");
    return 0;   /* m % -1 == 0; avoid overflow with 0x80000...%-1 */
  }
  else {
    lua_Integer r = m % n;
    if (r != 0 && (m ^ n) < 0)  /* 'm/n' would be non-integer negative? */
      r += n;  /* correct result for different rounding */
    return r;
  }
}


/* number of bits in an integer */
#define NBITS	cast_int(sizeof(lua_Integer) * CHAR_BIT)

/*
** Shift left operation. (Shift right just negates 'y'.)
*/
lua_Integer luaV_shiftl (lua_Integer x, lua_Integer y) {
  if (y < 0) {  /* shift right? */
    if (y <= -NBITS) return 0;
    else return intop(>>, x, -y);
  }
  else {  /* shift left */
    if (y >= NBITS) return 0;
    else return intop(<<, x, y);
  }
}


/*
** check whether cached closure in prototype 'p' may be reused, that is,
** whether there is a cached closure with the same upvalues needed by
** new closure to be created.
*/
static LClosure *getcached (Proto *p, UpVal **encup, StkId base) {
  LClosure *c = p->cache;
  if (c != NULL) {  /* is there a cached closure? */
    int nup = p->sizeupvalues;
    Upvaldesc *uv = p->upvalues;
    int i;
    for (i = 0; i < nup; i++) {  /* check whether it has right upvalues */
      TValue *v = uv[i].instack ? s2v(base + uv[i].idx) : encup[uv[i].idx]->v;
      if (c->upvals[i]->v != v)
        return NULL;  /* wrong upvalue; cannot reuse closure */
    }
    p->cachemiss = 0;  /* got a hit */
  }
  return c;  /* return cached closure (or NULL if no cached closure) */
}


/*
** create a new Lua closure, push it in the stack, and initialize
** its upvalues. ???
*/
static void pushclosure (lua_State *L, Proto *p, UpVal **encup, StkId base,
                         StkId ra) {
  int nup = p->sizeupvalues;
  Upvaldesc *uv = p->upvalues;
  int i;
  LClosure *ncl = luaF_newLclosure(L, nup);
  ncl->p = p;
  setclLvalue2s(L, ra, ncl);  /* anchor new closure in stack */
  for (i = 0; i < nup; i++) {  /* fill in its upvalues */
    if (uv[i].instack)  /* upvalue refers to local variable? */
      ncl->upvals[i] = luaF_findupval(L, base + uv[i].idx);
    else  /* get upvalue from enclosing function */
      ncl->upvals[i] = encup[uv[i].idx];
    luaC_objbarrier(L, ncl, ncl->upvals[i]);
  }
  if (p->cachemiss >= MAXMISS)  /* too many missings? */
    p->cache = NULL;  /* give up cache */
  else {
    p->cache = ncl;  /* save it on cache for reuse */
    luaC_protobarrier(L, p, ncl);
    p->cachemiss++;
  }
}


/*
** finish execution of an opcode interrupted by a yield
*/
void luaV_finishOp (lua_State *L) {
  CallInfo *ci = L->ci;
  StkId base = ci->func + 1;
  Instruction inst = *(ci->u.l.savedpc - 1);  /* interrupted instruction */
  OpCode op = GET_OPCODE(inst);
  switch (op) {  /* finish its execution */
    case OP_ADDI: case OP_SUBI:
    case OP_MULI: case OP_DIVI: case OP_IDIVI:
    case OP_MODI: case OP_POWI:
    case OP_ADD: case OP_SUB:
    case OP_MUL: case OP_DIV: case OP_IDIV:
    case OP_BANDK: case OP_BORK: case OP_BXORK:
    case OP_BAND: case OP_BOR: case OP_BXOR:
    case OP_SHRI: case OP_SHL: case OP_SHR:
    case OP_MOD: case OP_POW:
    case OP_UNM: case OP_BNOT: case OP_LEN:
    case OP_GETTABUP: case OP_GETTABLE: case OP_GETI:
    case OP_GETFIELD: case OP_SELF: {
      setobjs2s(L, base + GETARG_A(inst), --L->top);
      break;
    }
    case OP_LT: case OP_LE:
    case OP_LTI: case OP_LEI:
    case OP_GTI: case OP_GEI:
    case OP_EQ: {  /* note that 'OP_EQI'/'OP_EQK' cannot yield */
      int res = !l_isfalse(s2v(L->top - 1));
      L->top--;
      if (ci->callstatus & CIST_LEQ) {  /* "<=" using "<" instead? */
        ci->callstatus ^= CIST_LEQ;  /* clear mark */
        res = !res;  /* negate result */
      }
      lua_assert(GET_OPCODE(*ci->u.l.savedpc) == OP_JMP);
      if (res != GETARG_k(inst))  /* condition failed? */
        ci->u.l.savedpc++;  /* skip jump instruction */
      break;
    }
    case OP_CONCAT: {
      StkId top = L->top - 1;  /* top when 'luaT_trybinTM' was called */
      int a = GETARG_A(inst);      /* first element to concatenate */
      int total = cast_int(top - 1 - (base + a));  /* yet to concatenate */
      setobjs2s(L, top - 2, top);  /* put TM result in proper position */
      if (total > 1) {  /* are there elements to concat? */
        L->top = top - 1;  /* top is one after last element (at top-2) */
        luaV_concat(L, total);  /* concat them (may yield again) */
      }
      break;
    }
    case OP_TFORCALL: case OP_CALL: case OP_TAILCALL:
    case OP_SETTABUP: case OP_SETTABLE:
    case OP_SETI: case OP_SETFIELD:
      break;
    default: lua_assert(0);
  }
}




/*
** {==================================================================
** Function 'luaV_execute': main interpreter loop
** ===================================================================
*/


/*
** some macros for common tasks in 'luaV_execute'
*/


#define RA(i)	(base+GETARG_A(i))
#define RB(i)	(base+GETARG_B(i))
#define vRB(i)	s2v(RB(i))
#define KB(i)	(k+GETARG_B(i))
#define RC(i)	(base+GETARG_C(i))
#define vRC(i)	s2v(RC(i))
#define KC(i)	(k+GETARG_C(i))
#define RKC(i)	((TESTARG_k(i)) ? k + GETARG_C(i) : s2v(base + GETARG_C(i)))



#define updatetrap(ci)  (trap = ci->u.l.trap)

#define updatebase(ci)	(base = ci->func + 1)


/*
** Execute a jump instruction. The 'updatetrap' allows signals to stop
** tight loops. (Without it, the local copy of 'trap' could never change.)
*/
#define dojump(ci,i,e)	{ pc += GETARG_sJ(i) + e; updatetrap(ci); }


/* for test instructions, execute the jump instruction that follows it */
#define donextjump(ci)	{ i = *pc; dojump(ci, i, 1); }

/*
** do a conditional jump: skip next instruction if 'cond' is not what
** was expected (parameter 'k'), else do next instruction, which must
** be a jump.
*/
#define docondjump()	if (cond != GETARG_k(i)) pc++; else donextjump(ci);


/*
** Correct global 'pc'.
*/
#define savepc(L)	(ci->u.l.savedpc = pc)


/*
** Whenever code can raise errors, the global 'pc' and the global
** 'top' must be correct to report occasional errors.
*/
#define savestate(L,ci)		(savepc(L), L->top = ci->top)


/*
** Protect code that, in general, can raise errors, reallocate the
** stack, and change the hooks.
*/
#define Protect(exp)  (savestate(L,ci), (exp), updatetrap(ci))

/* special version that does not change the top */
#define ProtectNT(exp)  (savepc(L), (exp), updatetrap(ci))

/*
** Protect code that will finish the loop (returns).
*/
#define halfProtect(exp)  (savepc(L), (exp))


#define checkGC(L,c)  \
	{ luaC_condGC(L, L->top = (c),  /* limit of live values */ \
                         updatetrap(ci)); \
           luai_threadyield(L); }


/* fetch an instruction and prepare its execution */
#define vmfetch()	{ \
  if (trap) {  /* stack reallocation or hooks? */ \
    trap = luaG_traceexec(L, pc);  /* handle hooks */ \
    updatebase(ci);  /* correct stack */ \
  } \
  i = *(pc++); \
  ra = RA(i); /* WARNING: any stack reallocation invalidates 'ra' */ \
}

#define vmdispatch(o)	switch(o)
#define vmcase(l)	case l:
#define vmbreak		break


void luaV_execute (lua_State *L, CallInfo *ci) {
  LClosure *cl;
  TValue *k;
  StkId base;
  const Instruction *pc;
  int trap;
#if LUA_USE_JUMPTABLE
#include "ljumptab.h"
#endif
 tailcall:
  trap = L->hookmask;
  cl = clLvalue(s2v(ci->func));
  k = cl->p->k;
  pc = ci->u.l.savedpc;
  if (trap) {
    if (cl->p->is_vararg)
      trap = 0;  /* hooks will start after PREPVARARG instruction */
    else if (pc == cl->p->code)  /* first instruction (not resuming)? */
      luaD_hookcall(L, ci);
    ci->u.l.trap = 1;  /* there may be other hooks */
  }
  base = ci->func + 1;
  /* main loop of interpreter */
  for (;;) {
    int cond;  /* flag for conditional jumps */
    Instruction i;  /* instruction being executed */
    StkId ra;  /* instruction's A register */
    vmfetch();
    lua_assert(base == ci->func + 1);
    lua_assert(base <= L->top && L->top < L->stack + L->stacksize);
    lua_assert(ci->top < L->stack + L->stacksize);
    vmdispatch (GET_OPCODE(i)) {
      vmcase(OP_MOVE) {
        setobjs2s(L, ra, RB(i));
        vmbreak;
      }
      vmcase(OP_LOADK) {
        TValue *rb = k + GETARG_Bx(i);
        setobj2s(L, ra, rb);
        vmbreak;
      }
      vmcase(OP_LOADI) {
        lua_Integer b = GETARG_sBx(i);
        setivalue(s2v(ra), b);
        vmbreak;
      }
      vmcase(OP_LOADF) {
        int b = GETARG_sBx(i);
        setfltvalue(s2v(ra), cast_num(b));
        vmbreak;
      }
      vmcase(OP_LOADKX) {
        TValue *rb;
        rb = k + GETARG_Ax(*pc); pc++;
        setobj2s(L, ra, rb);
        vmbreak;
      }
      vmcase(OP_LOADBOOL) {
        setbvalue(s2v(ra), GETARG_B(i));
        if (GETARG_C(i)) pc++;  /* skip next instruction (if C) */
        vmbreak;
      }
      vmcase(OP_LOADNIL) {
        int b = GETARG_B(i);
        do {
          setnilvalue(s2v(ra++));
        } while (b--);
        vmbreak;
      }
      vmcase(OP_GETUPVAL) {
        int b = GETARG_B(i);
        setobj2s(L, ra, cl->upvals[b]->v);
        vmbreak;
      }
      vmcase(OP_SETUPVAL) {
        UpVal *uv = cl->upvals[GETARG_B(i)];
        setobj(L, uv->v, s2v(ra));
        luaC_barrier(L, uv, s2v(ra));
        vmbreak;
      }
      vmcase(OP_GETTABUP) {
        const TValue *slot;
        TValue *upval = cl->upvals[GETARG_B(i)]->v;
        TValue *rc = KC(i);
        TString *key = tsvalue(rc);  /* key must be a string */
        if (luaV_fastget(L, upval, key, slot, luaH_getshortstr)) {
          setobj2s(L, ra, slot);
        }
        else
          Protect(luaV_finishget(L, upval, rc, ra, slot));
        vmbreak;
      }
      vmcase(OP_GETTABLE) {
        const TValue *slot;
        TValue *rb = vRB(i);
        TValue *rc = vRC(i);
        lua_Unsigned n;
        if (ttisinteger(rc)  /* fast track for integers? */
            ? (n = ivalue(rc), luaV_fastgeti(L, rb, n, slot))
            : luaV_fastget(L, rb, rc, slot, luaH_get)) {
          setobj2s(L, ra, slot);
        }
        else
          Protect(luaV_finishget(L, rb, rc, ra, slot));
        vmbreak;
      }
      vmcase(OP_GETI) {
        const TValue *slot;
        TValue *rb = vRB(i);
        int c = GETARG_C(i);
        if (luaV_fastgeti(L, rb, c, slot)) {
          setobj2s(L, ra, slot);
        }
        else {
          TValue key;
          setivalue(&key, c);
          Protect(luaV_finishget(L, rb, &key, ra, slot));
        }
        vmbreak;
      }
      vmcase(OP_GETFIELD) {
        const TValue *slot;
        TValue *rb = vRB(i);
        TValue *rc = KC(i);
        TString *key = tsvalue(rc);  /* key must be a string */
        if (luaV_fastget(L, rb, key, slot, luaH_getshortstr)) {
          setobj2s(L, ra, slot);
        }
        else
          Protect(luaV_finishget(L, rb, rc, ra, slot));
        vmbreak;
      }
      vmcase(OP_SETTABUP) {
        const TValue *slot;
        TValue *upval = cl->upvals[GETARG_A(i)]->v;
        TValue *rb = KB(i);
        TValue *rc = RKC(i);
        TString *key = tsvalue(rb);  /* key must be a string */
        if (luaV_fastget(L, upval, key, slot, luaH_getshortstr)) {
          luaV_finishfastset(L, upval, slot, rc);
        }
        else
          Protect(luaV_finishset(L, upval, rb, rc, slot));
        vmbreak;
      }
      vmcase(OP_SETTABLE) {
        const TValue *slot;
        TValue *rb = vRB(i);  /* key (table is in 'ra') */
        TValue *rc = RKC(i);  /* value */
        lua_Unsigned n;
        if (ttisinteger(rb)  /* fast track for integers? */
            ? (n = ivalue(rb), luaV_fastgeti(L, s2v(ra), n, slot))
            : luaV_fastget(L, s2v(ra), rb, slot, luaH_get)) {
          luaV_finishfastset(L, s2v(ra), slot, rc);
        }
        else
          Protect(luaV_finishset(L, s2v(ra), rb, rc, slot));
        vmbreak;
      }
      vmcase(OP_SETI) {
        const TValue *slot;
        int c = GETARG_B(i);
        TValue *rc = RKC(i);
        if (luaV_fastgeti(L, s2v(ra), c, slot)) {
          luaV_finishfastset(L, s2v(ra), slot, rc);
        }
        else {
          TValue key;
          setivalue(&key, c);
          Protect(luaV_finishset(L, s2v(ra), &key, rc, slot));
        }
        vmbreak;
      }
      vmcase(OP_SETFIELD) {
        const TValue *slot;
        TValue *rb = KB(i);
        TValue *rc = RKC(i);
        TString *key = tsvalue(rb);  /* key must be a string */
        if (luaV_fastget(L, s2v(ra), key, slot, luaH_getshortstr)) {
          luaV_finishfastset(L, s2v(ra), slot, rc);
        }
        else
          Protect(luaV_finishset(L, s2v(ra), rb, rc, slot));
        vmbreak;
      }
      vmcase(OP_NEWTABLE) {
        int b = GETARG_B(i);
        int c = GETARG_C(i);
        Table *t;
        L->top = ci->top;  /* correct top in case of GC */
        t = luaH_new(L);  /* memory allocation */
        sethvalue2s(L, ra, t);
        if (b != 0 || c != 0)
          luaH_resize(L, t, luaO_fb2int(b), luaO_fb2int(c));  /* idem */
        checkGC(L, ra + 1);
        vmbreak;
      }
      vmcase(OP_SELF) {
        const TValue *slot;
        TValue *rb = vRB(i);
        TValue *rc = RKC(i);
        TString *key = tsvalue(rc);  /* key must be a string */
        setobj2s(L, ra + 1, rb);
        if (luaV_fastget(L, rb, key, slot, luaH_getstr)) {
          setobj2s(L, ra, slot);
        }
        else
          Protect(luaV_finishget(L, rb, rc, ra, slot));
        vmbreak;
      }
      vmcase(OP_ADDI) {
        TValue *rb = vRB(i);
        int ic = GETARG_sC(i);
        lua_Number nb;
        if (ttisinteger(rb)) {
          setivalue(s2v(ra), intop(+, ivalue(rb), ic));
        }
        else if (tonumberns(rb, nb)) {
          setfltvalue(s2v(ra), luai_numadd(L, nb, cast_num(ic)));
        }
        else
          Protect(luaT_trybiniTM(L, rb, ic, GETARG_k(i), ra, TM_ADD));
        vmbreak;
      }
      vmcase(OP_SUBI) {
        TValue *rb = vRB(i);
        int ic = GETARG_sC(i);
        lua_Number nb;
        if (ttisinteger(rb)) {
          setivalue(s2v(ra), intop(-, ivalue(rb), ic));
        }
        else if (tonumberns(rb, nb)) {
          setfltvalue(s2v(ra), luai_numsub(L, nb, cast_num(ic)));
        }
        else
          Protect(luaT_trybiniTM(L, rb, ic, 0, ra, TM_SUB));
        vmbreak;
      }
      vmcase(OP_MULI) {
        TValue *rb = vRB(i);
        int ic = GETARG_sC(i);
        lua_Number nb;
        if (ttisinteger(rb)) {
          setivalue(s2v(ra), intop(*, ivalue(rb), ic));
        }
        else if (tonumberns(rb, nb)) {
          setfltvalue(s2v(ra), luai_nummul(L, nb, cast_num(ic)));
        }
        else
          Protect(luaT_trybiniTM(L, rb, ic, GETARG_k(i), ra, TM_MUL));
        vmbreak;
      }
      vmcase(OP_MODI) {
        TValue *rb = vRB(i);
        int ic = GETARG_sC(i);
        lua_Number nb;
        if (ttisinteger(rb)) {
          setivalue(s2v(ra), luaV_mod(L, ivalue(rb), ic));
        }
        else if (tonumberns(rb, nb)) {
          lua_Number m;
          lua_Number nc = cast_num(ic);
          luai_nummod(L, nb, nc, m);
          setfltvalue(s2v(ra), m);
        }
        else
          Protect(luaT_trybiniTM(L, rb, ic, 0, ra, TM_MOD));
        vmbreak;
      }
      vmcase(OP_POWI) {
        TValue *rb = vRB(i);
        int ic = GETARG_sC(i);
        lua_Number nb;
        if (tonumberns(rb, nb)) {
          lua_Number nc = cast_num(ic);
          setfltvalue(s2v(ra), luai_numpow(L, nb, nc));
        }
        else
          Protect(luaT_trybiniTM(L, rb, ic, 0, ra, TM_POW));
        vmbreak;
      }
      vmcase(OP_DIVI) {
        TValue *rb = vRB(i);
        int ic = GETARG_sC(i);
        lua_Number nb;
        if (tonumberns(rb, nb)) {
          lua_Number nc = cast_num(ic);
          setfltvalue(s2v(ra), luai_numdiv(L, nb, nc));
        }
        else
          Protect(luaT_trybiniTM(L, rb, ic, 0, ra, TM_DIV));
        vmbreak;
      }
      vmcase(OP_IDIVI) {
        TValue *rb = vRB(i);
        int ic = GETARG_sC(i);
        lua_Number nb;
        if (ttisinteger(rb)) {
          setivalue(s2v(ra), luaV_div(L, ivalue(rb), ic));
        }
        else if (tonumberns(rb, nb)) {
          lua_Number nc = cast_num(ic);
          setfltvalue(s2v(ra), luai_numdiv(L, nb, nc));
        }
        else
          Protect(luaT_trybiniTM(L, rb, ic, 0, ra, TM_IDIV));
        vmbreak;
      }
      vmcase(OP_ADD) {
        TValue *rb = vRB(i);
        TValue *rc = vRC(i);
        lua_Number nb; lua_Number nc;
        if (ttisinteger(rb) && ttisinteger(rc)) {
          lua_Integer ib = ivalue(rb); lua_Integer ic = ivalue(rc);
          setivalue(s2v(ra), intop(+, ib, ic));
        }
        else if (tonumberns(rb, nb) && tonumberns(rc, nc)) {
          setfltvalue(s2v(ra), luai_numadd(L, nb, nc));
        }
        else
          Protect(luaT_trybinTM(L, rb, rc, ra, TM_ADD));
        vmbreak;
      }
      vmcase(OP_SUB) {
        TValue *rb = vRB(i);
        TValue *rc = vRC(i);
        lua_Number nb; lua_Number nc;
        if (ttisinteger(rb) && ttisinteger(rc)) {
          lua_Integer ib = ivalue(rb); lua_Integer ic = ivalue(rc);
          setivalue(s2v(ra), intop(-, ib, ic));
        }
        else if (tonumberns(rb, nb) && tonumberns(rc, nc)) {
          setfltvalue(s2v(ra), luai_numsub(L, nb, nc));
        }
        else
          Protect(luaT_trybinTM(L, rb, rc, ra, TM_SUB));
        vmbreak;
      }
      vmcase(OP_MUL) {
        TValue *rb = vRB(i);
        TValue *rc = vRC(i);
        lua_Number nb; lua_Number nc;
        if (ttisinteger(rb) && ttisinteger(rc)) {
          lua_Integer ib = ivalue(rb); lua_Integer ic = ivalue(rc);
          setivalue(s2v(ra), intop(*, ib, ic));
        }
        else if (tonumberns(rb, nb) && tonumberns(rc, nc)) {
          setfltvalue(s2v(ra), luai_nummul(L, nb, nc));
        }
        else
          Protect(luaT_trybinTM(L, rb, rc, ra, TM_MUL));
        vmbreak;
      }
      vmcase(OP_DIV) {  /* float division (always with floats) */
        TValue *rb = vRB(i);
        TValue *rc = vRC(i);
        lua_Number nb; lua_Number nc;
        if (tonumberns(rb, nb) && tonumberns(rc, nc)) {
          setfltvalue(s2v(ra), luai_numdiv(L, nb, nc));
        }
        else
          Protect(luaT_trybinTM(L, rb, rc, ra, TM_DIV));
        vmbreak;
      }
      vmcase(OP_BANDK) {
        TValue *p1 = vRB(i);
        TValue *p2 = KC(i);
        lua_Integer i1;
        if (tointegerns(p1, &i1)) {
          setivalue(s2v(ra), intop(&, i1, ivalue(p2)));
        }
        else
          Protect(luaT_trybinassocTM(L, p1, p2, ra, TESTARG_k(i), TM_BAND));
        vmbreak;
      }
      vmcase(OP_BORK) {
        TValue *p1 = vRB(i);
        TValue *p2 = KC(i);
        lua_Integer i1;
        if (tointegerns(p1, &i1)) {
          setivalue(s2v(ra), intop(|, i1, ivalue(p2)));
        }
        else
          Protect(luaT_trybinassocTM(L, p1, p2, ra, TESTARG_k(i), TM_BOR));
        vmbreak;
      }
      vmcase(OP_BXORK) {
        TValue *p1 = vRB(i);
        TValue *p2 = KC(i);
        lua_Integer i1;
        if (tointegerns(p1, &i1)) {
          setivalue(s2v(ra), intop(^, i1, ivalue(p2)));
        }
        else
          Protect(luaT_trybinassocTM(L, p1, p2, ra, TESTARG_k(i), TM_BXOR));
        vmbreak;
      }
      vmcase(OP_BAND) {
        TValue *rb = vRB(i);
        TValue *rc = vRC(i);
        lua_Integer ib; lua_Integer ic;
        if (tointegerns(rb, &ib) && tointegerns(rc, &ic)) {
          setivalue(s2v(ra), intop(&, ib, ic));
        }
        else
          Protect(luaT_trybinTM(L, rb, rc, ra, TM_BAND));
        vmbreak;
      }
      vmcase(OP_BOR) {
        TValue *rb = vRB(i);
        TValue *rc = vRC(i);
        lua_Integer ib; lua_Integer ic;
        if (tointegerns(rb, &ib) && tointegerns(rc, &ic)) {
          setivalue(s2v(ra), intop(|, ib, ic));
        }
        else
          Protect(luaT_trybinTM(L, rb, rc, ra, TM_BOR));
        vmbreak;
      }
      vmcase(OP_BXOR) {
        TValue *rb = vRB(i);
        TValue *rc = vRC(i);
        lua_Integer ib; lua_Integer ic;
        if (tointegerns(rb, &ib) && tointegerns(rc, &ic)) {
          setivalue(s2v(ra), intop(^, ib, ic));
        }
        else
          Protect(luaT_trybinTM(L, rb, rc, ra, TM_BXOR));
        vmbreak;
      }
      vmcase(OP_SHRI) {
        TValue *rb = vRB(i);
        int ic = GETARG_sC(i);
        lua_Integer ib;
        if (tointegerns(rb, &ib)) {
          setivalue(s2v(ra), luaV_shiftl(ib, -ic));
        }
        else {
          TMS ev = TM_SHR;
          if (TESTARG_k(i)) {
            ic = -ic;  ev = TM_SHL;
          }
          Protect(luaT_trybiniTM(L, rb, ic, 0, ra, ev));
        }
        vmbreak;
      }
      vmcase(OP_SHLI) {
        TValue *rb = vRB(i);
        int ic = GETARG_sC(i);
        lua_Integer ib;
        if (tointegerns(rb, &ib)) {
          setivalue(s2v(ra), luaV_shiftl(ic, ib));
        }
        else
          Protect(luaT_trybiniTM(L, rb, ic, 1, ra, TM_SHL));
        vmbreak;
      }
      vmcase(OP_SHL) {
        TValue *rb = vRB(i);
        TValue *rc = vRC(i);
        lua_Integer ib; lua_Integer ic;
        if (tointegerns(rb, &ib) && tointegerns(rc, &ic)) {
          setivalue(s2v(ra), luaV_shiftl(ib, ic));
        }
        else
          Protect(luaT_trybinTM(L, rb, rc, ra, TM_SHL));
        vmbreak;
      }
      vmcase(OP_SHR) {
        TValue *rb = vRB(i);
        TValue *rc = vRC(i);
        lua_Integer ib; lua_Integer ic;
        if (tointegerns(rb, &ib) && tointegerns(rc, &ic)) {
          setivalue(s2v(ra), luaV_shiftl(ib, -ic));
        }
        else
          Protect(luaT_trybinTM(L, rb, rc, ra, TM_SHR));
        vmbreak;
      }
      vmcase(OP_MOD) {
        TValue *rb = vRB(i);
        TValue *rc = vRC(i);
        lua_Number nb; lua_Number nc;
        if (ttisinteger(rb) && ttisinteger(rc)) {
          lua_Integer ib = ivalue(rb); lua_Integer ic = ivalue(rc);
          setivalue(s2v(ra), luaV_mod(L, ib, ic));
        }
        else if (tonumberns(rb, nb) && tonumberns(rc, nc)) {
          lua_Number m;
          luai_nummod(L, nb, nc, m);
          setfltvalue(s2v(ra), m);
        }
        else
          Protect(luaT_trybinTM(L, rb, rc, ra, TM_MOD));
        vmbreak;
      }
      vmcase(OP_IDIV) {  /* floor division */
        TValue *rb = vRB(i);
        TValue *rc = vRC(i);
        lua_Number nb; lua_Number nc;
        if (ttisinteger(rb) && ttisinteger(rc)) {
          lua_Integer ib = ivalue(rb); lua_Integer ic = ivalue(rc);
          setivalue(s2v(ra), luaV_div(L, ib, ic));
        }
        else if (tonumberns(rb, nb) && tonumberns(rc, nc)) {
          setfltvalue(s2v(ra), luai_numidiv(L, nb, nc));
        }
        else
          Protect(luaT_trybinTM(L, rb, rc, ra, TM_IDIV));
        vmbreak;
      }
      vmcase(OP_POW) {
        TValue *rb = vRB(i);
        TValue *rc = vRC(i);
        lua_Number nb; lua_Number nc;
        if (tonumberns(rb, nb) && tonumberns(rc, nc)) {
          setfltvalue(s2v(ra), luai_numpow(L, nb, nc));
        }
        else
          Protect(luaT_trybinTM(L, rb, rc, ra, TM_POW));
        vmbreak;
      }
      vmcase(OP_UNM) {
        TValue *rb = vRB(i);
        lua_Number nb;
        if (ttisinteger(rb)) {
          lua_Integer ib = ivalue(rb);
          setivalue(s2v(ra), intop(-, 0, ib));
        }
        else if (tonumberns(rb, nb)) {
          setfltvalue(s2v(ra), luai_numunm(L, nb));
        }
        else
          Protect(luaT_trybinTM(L, rb, rb, ra, TM_UNM));
        vmbreak;
      }
      vmcase(OP_BNOT) {
        TValue *rb = vRB(i);
        lua_Integer ib;
        if (tointegerns(rb, &ib)) {
          setivalue(s2v(ra), intop(^, ~l_castS2U(0), ib));
        }
        else
          Protect(luaT_trybinTM(L, rb, rb, ra, TM_BNOT));
        vmbreak;
      }
      vmcase(OP_NOT) {
        TValue *rb = vRB(i);
        int nrb = l_isfalse(rb);  /* next assignment may change this value */
        setbvalue(s2v(ra), nrb);
        vmbreak;
      }
      vmcase(OP_LEN) {
        Protect(luaV_objlen(L, ra, vRB(i)));
        vmbreak;
      }
      vmcase(OP_CONCAT) {
        int n = GETARG_B(i);  /* number of elements to concatenate */
        L->top = ra + n;  /* mark the end of concat operands */
        ProtectNT(luaV_concat(L, n));
        checkGC(L, L->top); /* 'luaV_concat' ensures correct top */
        vmbreak;
      }
      vmcase(OP_CLOSE) {
        luaF_close(L, ra);
        vmbreak;
      }
      vmcase(OP_JMP) {
        dojump(ci, i, 0);
        vmbreak;
      }
      vmcase(OP_EQ) {
        TValue *rb = vRB(i);
        Protect(cond = luaV_equalobj(L, s2v(ra), rb));
        docondjump();
        vmbreak;
      }
      vmcase(OP_LT) {
        TValue *rb = vRB(i);
        if (ttisinteger(s2v(ra)) && ttisinteger(rb))
          cond = (ivalue(s2v(ra)) < ivalue(rb));
        else if (ttisnumber(s2v(ra)) && ttisnumber(rb))
          cond = LTnum(s2v(ra), rb);
        else
          Protect(cond = lessthanothers(L, s2v(ra), rb));
        docondjump();
        vmbreak;
      }
      vmcase(OP_LE) {
        TValue *rb = vRB(i);
        if (ttisinteger(s2v(ra)) && ttisinteger(rb))
          cond = (ivalue(s2v(ra)) <= ivalue(rb));
        else if (ttisnumber(s2v(ra)) && ttisnumber(rb))
          cond = LEnum(s2v(ra), rb);
        else
          Protect(cond = lessequalothers(L, s2v(ra), rb));
        docondjump();
        vmbreak;
      }
      vmcase(OP_EQK) {
        TValue *rb = KB(i);
        /* basic types do not use '__eq'; we can use raw equality */
        cond = luaV_equalobj(NULL, s2v(ra), rb);
        docondjump();
        vmbreak;
      }
      vmcase(OP_EQI) {
        int im = GETARG_sB(i);
        if (ttisinteger(s2v(ra)))
          cond = (ivalue(s2v(ra)) == im);
        else if (ttisfloat(s2v(ra)))
          cond = luai_numeq(fltvalue(s2v(ra)), cast_num(im));
        else
          cond = 0;  /* other types cannot be equal to a number */
        docondjump();
        vmbreak;
      }
      vmcase(OP_LTI) {
        int im = GETARG_sB(i);
        if (ttisinteger(s2v(ra)))
          cond = (ivalue(s2v(ra)) < im);
        else if (ttisfloat(s2v(ra)))
          cond = luai_numlt(fltvalue(s2v(ra)), cast_num(im));
        else
          Protect(cond = luaT_callorderiTM(L, s2v(ra), im, 0, TM_LT));
        docondjump();
        vmbreak;
      }
      vmcase(OP_LEI) {
        int im = GETARG_sB(i);
        if (ttisinteger(s2v(ra)))
          cond = (ivalue(s2v(ra)) <= im);
        else if (ttisfloat(s2v(ra)))
          cond = luai_numle(fltvalue(s2v(ra)), cast_num(im));
        else
          Protect(cond = luaT_callorderiTM(L, s2v(ra), im, 0, TM_LE));
        docondjump();
        vmbreak;
      }
      vmcase(OP_GTI) {
        int im = GETARG_sB(i);
        if (ttisinteger(s2v(ra)))
          cond = (im < ivalue(s2v(ra)));
        else if (ttisfloat(s2v(ra)))
          cond = luai_numlt(cast_num(im), fltvalue(s2v(ra)));
        else
          Protect(cond = luaT_callorderiTM(L, s2v(ra), im, 1, TM_LT));
        docondjump();
        vmbreak;
      }
      vmcase(OP_GEI) {
        int im = GETARG_sB(i);
        if (ttisinteger(s2v(ra)))
          cond = (im <= ivalue(s2v(ra)));
        else if (ttisfloat(s2v(ra)))
          cond = luai_numle(cast_num(im), fltvalue(s2v(ra)));
        else
          Protect(cond = luaT_callorderiTM(L, s2v(ra), im, 1, TM_LE));
        docondjump();
        vmbreak;
      }
      vmcase(OP_TEST) {
        cond = !l_isfalse(s2v(ra));
        docondjump();
        vmbreak;
      }
      vmcase(OP_TESTSET) {
        TValue *rb = vRB(i);
        if (l_isfalse(rb) == GETARG_k(i))
          pc++;
        else {
          setobj2s(L, ra, rb);
          donextjump(ci);
        }
        vmbreak;
      }
      vmcase(OP_CALL) {
        int b = GETARG_B(i);
        int nresults = GETARG_C(i) - 1;
        if (b != 0)  /* fixed number of arguments? */
          L->top = ra + b;  /* top signals number of arguments */
        /* else previous instruction set top */
        ProtectNT(luaD_call(L, ra, nresults));
        vmbreak;
      }
      vmcase(OP_TAILCALL) {
        int b = GETARG_B(i);  /* number of arguments + 1 (function) */
        int delta = 0;  /* virtual 'func' - real 'func' (vararg functions) */
        if (b != 0)
          L->top = ra + b;
        else  /* previous instruction set top */
          b = cast_int(L->top - ra);
        savepc(ci);
        if (TESTARG_k(i)) {
          int nparams1 = GETARG_C(i);
          if (nparams1)  /* vararg function? */
            delta = ci->u.l.nextraargs + nparams1;
          luaF_close(L, base);  /* close upvalues from current call */
        }
        if (!ttisfunction(s2v(ra))) {  /* not a function? */
          luaD_tryfuncTM(L, ra);  /* try '__call' metamethod */
          b++;  /* there is now one extra argument */
        }
        if (!ttisLclosure(s2v(ra))) {  /* C function? */
          luaD_call(L, ra, LUA_MULTRET);  /* call it */
          updatetrap(ci);
          if (trap) {  /* stack may have been relocated */
            updatebase(ci);
            ra = RA(i);
          }
          ci->func -= delta;
          luaD_poscall(L, ci, cast_int(L->top - ra));
          return;
        }
        else {  /* Lua tail call */
          ci->func -= delta;
          luaD_pretailcall(L, ci, ra, b);  /* prepare call frame */
          goto tailcall;
        }
        vmbreak;
      }
      vmcase(OP_RETURN) {
        int n = GETARG_B(i) - 1;  /* number of results */
        if (n < 0)  /* not fixed? */
          n = cast_int(L->top - ra);  /* get what is available */
        else
          L->top = ra + n;  /* set call for 'luaD_poscall' */
        if (TESTARG_k(i)) {
          int nparams1 = GETARG_C(i);
          if (nparams1)  /* vararg function? */
            ci->func -= ci->u.l.nextraargs + nparams1;
          luaF_close(L, base);  /* there may be open upvalues */
        }
        halfProtect(luaD_poscall(L, ci, n));
        return;
      }
      vmcase(OP_RETURN0) {
        if (L->hookmask) {
          L->top = ra;
          halfProtect(luaD_poscall(L, ci, 0));  /* no hurry... */
        }
        else {
          int nres = ci->nresults;
          L->ci = ci->previous;  /* back to caller */
          L->top = base - 1;
          while (nres-- > 0)
            setnilvalue(s2v(L->top++));  /* all results are nil */
        }
        return;
      }
      vmcase(OP_RETURN1) {
        if (L->hookmask) {
          L->top = ra + 1;
          halfProtect(luaD_poscall(L, ci, 1));  /* no hurry... */
        }
        else {
          int nres = ci->nresults;
          L->ci = ci->previous;  /* back to caller */
          if (nres == 0)
            L->top = base - 1;  /* asked for no results */
          else {
            setobjs2s(L, base - 1, ra);  /* at least this result */
            L->top = base;
            while (--nres > 0)  /* complete missing results */
              setnilvalue(s2v(L->top++));
          }
        }
        return;
      }
      vmcase(OP_FORLOOP1) {
        lua_Integer idx = intop(+, ivalue(s2v(ra)), 1); /* increment index */
        lua_Integer limit = ivalue(s2v(ra + 1));
        if (idx <= limit) {
          pc -= GETARG_Bx(i);  /* jump back */
          chgivalue(s2v(ra), idx);  /* update internal index... */
          setivalue(s2v(ra + 3), idx);  /* ...and external index */
        }
        updatetrap(ci);
        vmbreak;
      }
      vmcase(OP_FORPREP1) {
        TValue *init = s2v(ra);
        TValue *plimit = s2v(ra + 1);
        lua_Integer ilimit, initv;
        int stopnow;
        if (unlikely(!forlimit(plimit, &ilimit, 1, &stopnow))) {
            savestate(L, ci);  /* for the error message */
            luaG_runerror(L, "'for' limit must be a number");
        }
        initv = (stopnow ? 0 : ivalue(init));
        setivalue(plimit, ilimit);
        setivalue(init, intop(-, initv, 1));
        pc += GETARG_Bx(i);
        vmbreak;
      }
      vmcase(OP_FORLOOP) {
        if (ttisinteger(s2v(ra))) {  /* integer loop? */
          lua_Integer step = ivalue(s2v(ra + 2));
          lua_Integer idx = intop(+, ivalue(s2v(ra)), step); /* new index */
          lua_Integer limit = ivalue(s2v(ra + 1));
          if ((0 < step) ? (idx <= limit) : (limit <= idx)) {
            pc -= GETARG_Bx(i);  /* jump back */
            chgivalue(s2v(ra), idx);  /* update internal index... */
            setivalue(s2v(ra + 3), idx);  /* ...and external index */
          }
        }
        else {  /* floating loop */
          lua_Number step = fltvalue(s2v(ra + 2));
          lua_Number limit = fltvalue(s2v(ra + 1));
          lua_Number idx = fltvalue(s2v(ra));
          idx = luai_numadd(L, idx, step);  /* inc. index */
          if (luai_numlt(0, step) ? luai_numle(idx, limit)
                                  : luai_numle(limit, idx)) {
            pc -= GETARG_Bx(i);  /* jump back */
            chgfltvalue(s2v(ra), idx);  /* update internal index... */
            setfltvalue(s2v(ra + 3), idx);  /* ...and external index */
          }
        }
        updatetrap(ci);
        vmbreak;
      }
      vmcase(OP_FORPREP) {
        TValue *init = s2v(ra);
        TValue *plimit = s2v(ra + 1);
        TValue *pstep = s2v(ra + 2);
        lua_Integer ilimit;
        int stopnow;
        if (ttisinteger(init) && ttisinteger(pstep) &&
            forlimit(plimit, &ilimit, ivalue(pstep), &stopnow)) {
          /* all values are integer */
          lua_Integer initv = (stopnow ? 0 : ivalue(init));
          setivalue(plimit, ilimit);
          setivalue(init, intop(-, initv, ivalue(pstep)));
        }
        else {  /* try making all values floats */
          lua_Number ninit; lua_Number nlimit; lua_Number nstep;
          savestate(L, ci);  /* in case of errors */
          if (unlikely(!tonumber(plimit, &nlimit)))
            luaG_runerror(L, "'for' limit must be a number");
          setfltvalue(plimit, nlimit);
          if (unlikely(!tonumber(pstep, &nstep)))
            luaG_runerror(L, "'for' step must be a number");
          setfltvalue(pstep, nstep);
          if (unlikely(!tonumber(init, &ninit)))
            luaG_runerror(L, "'for' initial value must be a number");
          setfltvalue(init, luai_numsub(L, ninit, nstep));
        }
        pc += GETARG_Bx(i);
        vmbreak;
      }
      vmcase(OP_TFORCALL) {
        StkId cb = ra + 3;  /* call base */
        setobjs2s(L, cb+2, ra+2);
        setobjs2s(L, cb+1, ra+1);
        setobjs2s(L, cb, ra);
        L->top = cb + 3;  /* func. + 2 args (state and index) */
        Protect(luaD_call(L, cb, GETARG_C(i)));
        if (trap)  /* keep 'base' correct for next instruction */
          updatebase(ci);
        i = *(pc++);  /* go to next instruction */
        ra = RA(i);  /* get its 'ra' */
        lua_assert(GET_OPCODE(i) == OP_TFORLOOP);
        goto l_tforloop;
      }
      vmcase(OP_TFORLOOP) {
        l_tforloop:
        if (!ttisnil(s2v(ra + 1))) {  /* continue loop? */
          setobjs2s(L, ra, ra + 1);  /* save control variable */
          pc -= GETARG_Bx(i);  /* jump back */
        }
        vmbreak;
      }
      vmcase(OP_SETLIST) {
        int n = GETARG_B(i);
        int c = GETARG_C(i);
        unsigned int last;
        Table *h;
        if (n == 0)
          n = cast_int(L->top - ra) - 1;
        else
          L->top = ci->top;  /* correct top in case of GC */
        if (c == 0) {
          c = GETARG_Ax(*pc); pc++;
        }
        h = hvalue(s2v(ra));
        last = ((c-1)*LFIELDS_PER_FLUSH) + n;
        if (last > luaH_realasize(h))  /* needs more space? */
          luaH_resizearray(L, h, last);  /* preallocate it at once */
        for (; n > 0; n--) {
          TValue *val = s2v(ra + n);
          setobj2t(L, &h->array[last - 1], val);
          last--;
          luaC_barrierback(L, obj2gco(h), val);
        }
        vmbreak;
      }
      vmcase(OP_CLOSURE) {
        Proto *p = cl->p->p[GETARG_Bx(i)];
        LClosure *ncl = getcached(p, cl->upvals, base);  /* cached closure */
        if (ncl == NULL) {  /* no match? */
          savestate(L, ci);  /* in case of allocation errors */
          pushclosure(L, p, cl->upvals, base, ra);  /* create a new one */
        }
        else
          setclLvalue2s(L, ra, ncl);  /* push cashed closure */
        checkGC(L, ra + 1);
        vmbreak;
      }
      vmcase(OP_VARARG) {
        int n = GETARG_C(i) - 1;  /* required results */
        Protect(luaT_getvarargs(L, ci, ra, n));
        vmbreak;
      }
      vmcase(OP_PREPVARARG) {
        luaT_adjustvarargs(L, GETARG_A(i), ci, cl->p);
        updatetrap(ci);
        if (trap) {
          luaD_hookcall(L, ci);
          L->oldpc = pc + 1;  /* next opcode will be seen as a "new" line */
        }
        updatebase(ci);  /* function has new base after adjustment */
        vmbreak;
      }
      vmcase(OP_EXTRAARG) {
        lua_assert(0);
        vmbreak;
      }
    }
  }
}

/* }================================================================== */

