/*
** $Id: lmathlib.c,v 1.124 2018/03/11 14:48:09 roberto Exp roberto $
** Standard mathematical library
** See Copyright Notice in lua.h
*/

#define lmathlib_c
#define LUA_LIB

#include "lprefix.h"


#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"


#undef PI
#define PI	(l_mathop(3.141592653589793238462643383279502884))


static int math_abs (lua_State *L) {
  if (lua_isinteger(L, 1)) {
    lua_Integer n = lua_tointeger(L, 1);
    if (n < 0) n = (lua_Integer)(0u - (lua_Unsigned)n);
    lua_pushinteger(L, n);
  }
  else
    lua_pushnumber(L, l_mathop(fabs)(luaL_checknumber(L, 1)));
  return 1;
}

static int math_sin (lua_State *L) {
  lua_pushnumber(L, l_mathop(sin)(luaL_checknumber(L, 1)));
  return 1;
}

static int math_cos (lua_State *L) {
  lua_pushnumber(L, l_mathop(cos)(luaL_checknumber(L, 1)));
  return 1;
}

static int math_tan (lua_State *L) {
  lua_pushnumber(L, l_mathop(tan)(luaL_checknumber(L, 1)));
  return 1;
}

static int math_asin (lua_State *L) {
  lua_pushnumber(L, l_mathop(asin)(luaL_checknumber(L, 1)));
  return 1;
}

static int math_acos (lua_State *L) {
  lua_pushnumber(L, l_mathop(acos)(luaL_checknumber(L, 1)));
  return 1;
}

static int math_atan (lua_State *L) {
  lua_Number y = luaL_checknumber(L, 1);
  lua_Number x = luaL_optnumber(L, 2, 1);
  lua_pushnumber(L, l_mathop(atan2)(y, x));
  return 1;
}


static int math_toint (lua_State *L) {
  int valid;
  lua_Integer n = lua_tointegerx(L, 1, &valid);
  if (valid)
    lua_pushinteger(L, n);
  else {
    luaL_checkany(L, 1);
    lua_pushnil(L);  /* value is not convertible to integer */
  }
  return 1;
}


static void pushnumint (lua_State *L, lua_Number d) {
  lua_Integer n;
  if (lua_numbertointeger(d, &n))  /* does 'd' fit in an integer? */
    lua_pushinteger(L, n);  /* result is integer */
  else
    lua_pushnumber(L, d);  /* result is float */
}


static int math_floor (lua_State *L) {
  if (lua_isinteger(L, 1))
    lua_settop(L, 1);  /* integer is its own floor */
  else {
    lua_Number d = l_mathop(floor)(luaL_checknumber(L, 1));
    pushnumint(L, d);
  }
  return 1;
}


static int math_ceil (lua_State *L) {
  if (lua_isinteger(L, 1))
    lua_settop(L, 1);  /* integer is its own ceil */
  else {
    lua_Number d = l_mathop(ceil)(luaL_checknumber(L, 1));
    pushnumint(L, d);
  }
  return 1;
}


static int math_fmod (lua_State *L) {
  if (lua_isinteger(L, 1) && lua_isinteger(L, 2)) {
    lua_Integer d = lua_tointeger(L, 2);
    if ((lua_Unsigned)d + 1u <= 1u) {  /* special cases: -1 or 0 */
      luaL_argcheck(L, d != 0, 2, "zero");
      lua_pushinteger(L, 0);  /* avoid overflow with 0x80000... / -1 */
    }
    else
      lua_pushinteger(L, lua_tointeger(L, 1) % d);
  }
  else
    lua_pushnumber(L, l_mathop(fmod)(luaL_checknumber(L, 1),
                                     luaL_checknumber(L, 2)));
  return 1;
}


/*
** next function does not use 'modf', avoiding problems with 'double*'
** (which is not compatible with 'float*') when lua_Number is not
** 'double'.
*/
static int math_modf (lua_State *L) {
  if (lua_isinteger(L ,1)) {
    lua_settop(L, 1);  /* number is its own integer part */
    lua_pushnumber(L, 0);  /* no fractional part */
  }
  else {
    lua_Number n = luaL_checknumber(L, 1);
    /* integer part (rounds toward zero) */
    lua_Number ip = (n < 0) ? l_mathop(ceil)(n) : l_mathop(floor)(n);
    pushnumint(L, ip);
    /* fractional part (test needed for inf/-inf) */
    lua_pushnumber(L, (n == ip) ? l_mathop(0.0) : (n - ip));
  }
  return 2;
}


static int math_sqrt (lua_State *L) {
  lua_pushnumber(L, l_mathop(sqrt)(luaL_checknumber(L, 1)));
  return 1;
}


static int math_ult (lua_State *L) {
  lua_Integer a = luaL_checkinteger(L, 1);
  lua_Integer b = luaL_checkinteger(L, 2);
  lua_pushboolean(L, (lua_Unsigned)a < (lua_Unsigned)b);
  return 1;
}

static int math_log (lua_State *L) {
  lua_Number x = luaL_checknumber(L, 1);
  lua_Number res;
  if (lua_isnoneornil(L, 2))
    res = l_mathop(log)(x);
  else {
    lua_Number base = luaL_checknumber(L, 2);
#if !defined(LUA_USE_C89)
    if (base == l_mathop(2.0))
      res = l_mathop(log2)(x); else
#endif
    if (base == l_mathop(10.0))
      res = l_mathop(log10)(x);
    else
      res = l_mathop(log)(x)/l_mathop(log)(base);
  }
  lua_pushnumber(L, res);
  return 1;
}

static int math_exp (lua_State *L) {
  lua_pushnumber(L, l_mathop(exp)(luaL_checknumber(L, 1)));
  return 1;
}

static int math_deg (lua_State *L) {
  lua_pushnumber(L, luaL_checknumber(L, 1) * (l_mathop(180.0) / PI));
  return 1;
}

static int math_rad (lua_State *L) {
  lua_pushnumber(L, luaL_checknumber(L, 1) * (PI / l_mathop(180.0)));
  return 1;
}


static int math_min (lua_State *L) {
  int n = lua_gettop(L);  /* number of arguments */
  int imin = 1;  /* index of current minimum value */
  int i;
  luaL_argcheck(L, n >= 1, 1, "value expected");
  for (i = 2; i <= n; i++) {
    if (lua_compare(L, i, imin, LUA_OPLT))
      imin = i;
  }
  lua_pushvalue(L, imin);
  return 1;
}


static int math_max (lua_State *L) {
  int n = lua_gettop(L);  /* number of arguments */
  int imax = 1;  /* index of current maximum value */
  int i;
  luaL_argcheck(L, n >= 1, 1, "value expected");
  for (i = 2; i <= n; i++) {
    if (lua_compare(L, imax, i, LUA_OPLT))
      imax = i;
  }
  lua_pushvalue(L, imax);
  return 1;
}


static int math_type (lua_State *L) {
  if (lua_type(L, 1) == LUA_TNUMBER) {
      if (lua_isinteger(L, 1))
        lua_pushliteral(L, "integer");
      else
        lua_pushliteral(L, "float");
  }
  else {
    luaL_checkany(L, 1);
    lua_pushnil(L);
  }
  return 1;
}



/*
** {==================================================================
** Pseudo-Random Number Generator based on 'xorshift128+'.
** ===================================================================
*/

/* number of binary digits in the mantissa of a float */
#define FIGS	l_mathlim(MANT_DIG)

#if FIGS > 64
/* there are only 64 random bits; use them all */
#undef FIGS
#define FIGS	64
#endif


#if !defined(LUA_USE_C89) && defined(LLONG_MAX) && !defined(LUA_DEBUG)  /* { */

/*
** Assume long long.
*/

/* a 64-bit value */
typedef unsigned long long I;

static I xorshift128plus (I *state) {
  I x = state[0];
  I y = state[1];
  state[0] = y;
  x ^= x << 23;
  state[1] = (x ^ (x >> 18)) ^ (y ^ (y >> 5));
  return state[1] + y;
}

/* must take care to not shift stuff by more than 63 slots */

#define maskFIG		(~(~1LLU << (FIGS - 1)))  /* use FIGS bits */
#define shiftFIG	(l_mathop(0.5) / (1LLU << (FIGS - 1)))  /* 2^(-FIG) */

/*
** Convert bits from a random integer into a float in the
** interval [0,1).
*/
static lua_Number I2d (I x) {
  return (lua_Number)(x & maskFIG) * shiftFIG;
}

/* convert an 'I' to a lua_Unsigned */
#define I2UInt(x)	((lua_Unsigned)(x))

/* convert a lua_Integer to an 'I' */
#define Int2I(x)	((I)(x))

#else  /* no long long   }{ */

/*
** Use two 32-bit integers to represent a 64-bit quantity.
*/

#if LUAI_BITSINT >= 32
typedef unsigned int lu_int32;
#else
typedef unsigned long lu_int32;
#endif

/* a 64-bit value */
typedef struct I {
  lu_int32 h;  /* higher half */
  lu_int32 l;  /* lower half */
} I;


/*
** basic operations on 'I' values
*/

static I pack (lu_int32 h, lu_int32 l) {
  I result;
  result.h = h;
  result.l = l;
  return result;
}

/* i ^ (i << n) */
static I Ixorshl (I i, int n) {
  return pack(i.h ^ ((i.h << n) | (i.l >> (32 - n))), i.l ^ (i.l << n));
}

/* i ^ (i >> n) */
static I Ixorshr (I i, int n) {
  return pack(i.h ^ (i.h >> n), i.l ^ ((i.l >> n) | (i.h << (32 - n))));
}

static I Ixor (I i1, I i2) {
  return pack(i1.h ^ i2.h, i1.l ^ i2.l);
}

static I Iadd (I i1, I i2) {
  I result = pack(i1.h + i2.h, i1.l + i2.l);
  if (result.l < i1.l)  /* carry? */
    result.h++;
  return result;
}


/*
** implementation of 'xorshift128+' algorithm on 'I' values
*/
static I xorshift128plus (I *state) {
  I x = state[0];
  I y = state[1];
  state[0] = y;
  x = Ixorshl(x, 23);  /* x ^= x << 23; */
  /* state[1] = (x ^ (x >> 18)) ^ (y ^ (y >> 5)); */
  state[1] = Ixor(Ixorshr(x, 18), Ixorshr(y, 5));
  return Iadd(state[1], y);  /* return state[1] + y; */
}


/*
** Converts an 'I' into a float.
*/

/* an unsigned 1 with proper type */
#define UONE		((lu_int32)1)

#if FIGS <= 32

#define maskHF		0  /* do not need bits from higher half */
#define maskLOW		(~(~UONE << (FIGS - 1)))  /* use FIG bits */
#define shiftFIG	(l_mathop(0.5) / (UONE << (FIGS - 1)))  /* 2^(-FIG) */

#else	/* 32 < FIGS <= 64 */

/* must take care to not shift stuff by more than 31 slots */

/* use FIG - 32 bits from higher half */
#define maskHF		(~(~UONE << (FIGS - 33)))

/* use all bits from lower half */
#define maskLOW		(~(lu_int32)0)

/* 2^(-FIG) == (1 / 2^33) / 2^(FIG-33) */
#define shiftFIG  ((lua_Number)(1.0 / 8589934592.0) / (UONE << (FIGS - 33)))

#endif

#define twoto32		l_mathop(4294967296.0)  /* 2^32 */

static lua_Number I2d (I x) {
  lua_Number h = (lua_Number)(x.h & maskHF);
  lua_Number l = (lua_Number)(x.l & maskLOW);
  return (h * twoto32 + l) * shiftFIG;
}

static lua_Unsigned I2UInt (I x) {
  return ((lua_Unsigned)x.h << 31 << 1) | x.l;
}

static I Int2I (lua_Integer n) {
  lua_Unsigned un = n;
  return pack((lu_int32)un, (lu_int32)(un >> 31 >> 1));
}

#endif  /* } */


/*
** A state uses two 'I' values.
*/
typedef struct {
  I s[2];
} RanState;


/*
** Project the random integer 'ran' into the interval [0, n].
** Because 'ran' has 2^B possible values, the projection can only
** be uniform when the size of the interval [0, n] is a power of 2
** (exact division). With the fairest possible projection (e.g.,
** '(ran % (n + 1))'), the maximum bias is 1 in 2^B/n.
** For a "small" 'n', this bias is acceptable. (Here, we accept
** a maximum bias of 0.0001%.) For a larger 'n', we first
** compute 'lim', the smallest (2^b - 1) not smaller than 'n',
** to get a uniform projection into [0,lim]. If the result is
** inside [0, n], we are done. Otherwise, we try we another
** 'ran' until we have a result inside the interval.
*/

#define MAXBIAS		1000000

static lua_Unsigned project (lua_Unsigned ran, lua_Unsigned n,
                             RanState *state) {
  if (n < LUA_MAXUNSIGNED / MAXBIAS)
    return ran % (n + 1);
  else {
    /* compute the smallest (2^b - 1) not smaller than 'n' */
    lua_Unsigned lim = n;
    lim |= (lim >> 1);
    lim |= (lim >> 2);
    lim |= (lim >> 4);
    lim |= (lim >> 8);
    lim |= (lim >> 16);
#if (LUA_MAXINTEGER >> 30 >> 2) > 0
    lim |= (lim >> 32);  /* integer type has more than 32 bits */
#endif
    lua_assert((lim & (lim + 1)) == 0  /* 'lim + 1' is a power of 2 */
      && lim >= n  /* not smaller than 'n' */
      && (lim >> 1) < n);  /* it is the smallest one */
    while ((ran & lim) > n)
      ran = I2UInt(xorshift128plus(state->s));
    return ran & lim;
  }
}


static int math_random (lua_State *L) {
  lua_Integer low, up;
  lua_Unsigned p;
  RanState *state = (RanState *)lua_touserdata(L, lua_upvalueindex(1));
  I rv = xorshift128plus(state->s);  /* next pseudo-random value */
  switch (lua_gettop(L)) {  /* check number of arguments */
    case 0: {  /* no arguments */
      lua_pushnumber(L, I2d(rv));  /* float between 0 and 1 */
      return 1;
    }
    case 1: {  /* only upper limit */
      low = 1;
      up = luaL_checkinteger(L, 1);
      if (up == 0) {  /* single 0 as argument? */
        lua_pushinteger(L, I2UInt(rv));  /* full random integer */
        return 1;
      }
      break;
    }
    case 2: {  /* lower and upper limits */
      low = luaL_checkinteger(L, 1);
      up = luaL_checkinteger(L, 2);
      break;
    }
    default: return luaL_error(L, "wrong number of arguments");
  }
  /* random integer in the interval [low, up] */
  luaL_argcheck(L, low <= up, 1, "interval is empty");
  /* project random integer into the interval [0, up - low] */
  p = project(I2UInt(rv), (lua_Unsigned)up - (lua_Unsigned)low, state);
  lua_pushinteger(L, p + (lua_Unsigned)low);
  return 1;
}


static void setseed (I *state, lua_Integer n) {
  int i;
  state[0] = Int2I(n);
  state[1] = Int2I(~n);
  for (i = 0; i < 16; i++)
    xorshift128plus(state);  /* discard initial values */
}


static int math_randomseed (lua_State *L) {
  RanState *state = (RanState *)lua_touserdata(L, lua_upvalueindex(1));
  lua_Integer n = luaL_checkinteger(L, 1);
  setseed(state->s, n);
  return 0;
}


static const luaL_Reg randfuncs[] = {
  {"random", math_random},
  {"randomseed", math_randomseed},
  {NULL, NULL}
};

static void setrandfunc (lua_State *L) {
  RanState *state = (RanState *)lua_newuserdatauv(L, sizeof(RanState), 0);
  setseed(state->s, 0);
  luaL_setfuncs(L, randfuncs, 1);
}

/* }================================================================== */


/*
** {==================================================================
** Deprecated functions (for compatibility only)
** ===================================================================
*/
#if defined(LUA_COMPAT_MATHLIB)

static int math_cosh (lua_State *L) {
  lua_pushnumber(L, l_mathop(cosh)(luaL_checknumber(L, 1)));
  return 1;
}

static int math_sinh (lua_State *L) {
  lua_pushnumber(L, l_mathop(sinh)(luaL_checknumber(L, 1)));
  return 1;
}

static int math_tanh (lua_State *L) {
  lua_pushnumber(L, l_mathop(tanh)(luaL_checknumber(L, 1)));
  return 1;
}

static int math_pow (lua_State *L) {
  lua_Number x = luaL_checknumber(L, 1);
  lua_Number y = luaL_checknumber(L, 2);
  lua_pushnumber(L, l_mathop(pow)(x, y));
  return 1;
}

static int math_frexp (lua_State *L) {
  int e;
  lua_pushnumber(L, l_mathop(frexp)(luaL_checknumber(L, 1), &e));
  lua_pushinteger(L, e);
  return 2;
}

static int math_ldexp (lua_State *L) {
  lua_Number x = luaL_checknumber(L, 1);
  int ep = (int)luaL_checkinteger(L, 2);
  lua_pushnumber(L, l_mathop(ldexp)(x, ep));
  return 1;
}

static int math_log10 (lua_State *L) {
  lua_pushnumber(L, l_mathop(log10)(luaL_checknumber(L, 1)));
  return 1;
}

#endif
/* }================================================================== */



static const luaL_Reg mathlib[] = {
  {"abs",   math_abs},
  {"acos",  math_acos},
  {"asin",  math_asin},
  {"atan",  math_atan},
  {"ceil",  math_ceil},
  {"cos",   math_cos},
  {"deg",   math_deg},
  {"exp",   math_exp},
  {"tointeger", math_toint},
  {"floor", math_floor},
  {"fmod",   math_fmod},
  {"ult",   math_ult},
  {"log",   math_log},
  {"max",   math_max},
  {"min",   math_min},
  {"modf",   math_modf},
  {"rad",   math_rad},
  {"sin",   math_sin},
  {"sqrt",  math_sqrt},
  {"tan",   math_tan},
  {"type", math_type},
#if defined(LUA_COMPAT_MATHLIB)
  {"atan2", math_atan},
  {"cosh",   math_cosh},
  {"sinh",   math_sinh},
  {"tanh",   math_tanh},
  {"pow",   math_pow},
  {"frexp", math_frexp},
  {"ldexp", math_ldexp},
  {"log10", math_log10},
#endif
  /* placeholders */
  {"random", NULL},
  {"randomseed", NULL},
  {"pi", NULL},
  {"huge", NULL},
  {"maxinteger", NULL},
  {"mininteger", NULL},
  {NULL, NULL}
};


/*
** Open math library
*/
LUAMOD_API int luaopen_math (lua_State *L) {
  luaL_newlib(L, mathlib);
  lua_pushnumber(L, PI);
  lua_setfield(L, -2, "pi");
  lua_pushnumber(L, (lua_Number)HUGE_VAL);
  lua_setfield(L, -2, "huge");
  lua_pushinteger(L, LUA_MAXINTEGER);
  lua_setfield(L, -2, "maxinteger");
  lua_pushinteger(L, LUA_MININTEGER);
  lua_setfield(L, -2, "mininteger");
  setrandfunc(L);
  return 1;
}

