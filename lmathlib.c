/*
** $Id: lmathlib.c,v 1.37 2001/03/06 20:09:38 roberto Exp roberto $
** Standard mathematical library
** See Copyright Notice in lua.h
*/


#include <stdlib.h>
#include <math.h>

#define LUA_PRIVATE
#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"


#undef PI
#define PI (3.14159265358979323846)
#define RADIANS_PER_DEGREE (PI/180.0)



/*
** If you want Lua to operate in radians (instead of degrees),
** define RADIANS
*/
#ifdef RADIANS
#define FROMRAD(a)	(a)
#define TORAD(a)	(a)
#else
#define FROMRAD(a)	((a)/RADIANS_PER_DEGREE)
#define TORAD(a)	((a)*RADIANS_PER_DEGREE)
#endif


static int math_abs (lua_State *L) {
  lua_pushnumber(L, fabs(luaL_check_number(L, 1)));
  return 1;
}

static int math_sin (lua_State *L) {
  lua_pushnumber(L, sin(TORAD(luaL_check_number(L, 1))));
  return 1;
}

static int math_cos (lua_State *L) {
  lua_pushnumber(L, cos(TORAD(luaL_check_number(L, 1))));
  return 1;
}

static int math_tan (lua_State *L) {
  lua_pushnumber(L, tan(TORAD(luaL_check_number(L, 1))));
  return 1;
}

static int math_asin (lua_State *L) {
  lua_pushnumber(L, FROMRAD(asin(luaL_check_number(L, 1))));
  return 1;
}

static int math_acos (lua_State *L) {
  lua_pushnumber(L, FROMRAD(acos(luaL_check_number(L, 1))));
  return 1;
}

static int math_atan (lua_State *L) {
  lua_pushnumber(L, FROMRAD(atan(luaL_check_number(L, 1))));
  return 1;
}

static int math_atan2 (lua_State *L) {
  lua_pushnumber(L, FROMRAD(atan2(luaL_check_number(L, 1), luaL_check_number(L, 2))));
  return 1;
}

static int math_ceil (lua_State *L) {
  lua_pushnumber(L, ceil(luaL_check_number(L, 1)));
  return 1;
}

static int math_floor (lua_State *L) {
  lua_pushnumber(L, floor(luaL_check_number(L, 1)));
  return 1;
}

static int math_mod (lua_State *L) {
  lua_pushnumber(L, fmod(luaL_check_number(L, 1), luaL_check_number(L, 2)));
  return 1;
}

static int math_sqrt (lua_State *L) {
  lua_pushnumber(L, sqrt(luaL_check_number(L, 1)));
  return 1;
}

static int math_pow (lua_State *L) {
  lua_pushnumber(L, pow(luaL_check_number(L, 1), luaL_check_number(L, 2)));
  return 1;
}

static int math_log (lua_State *L) {
  lua_pushnumber(L, log(luaL_check_number(L, 1)));
  return 1;
}

static int math_log10 (lua_State *L) {
  lua_pushnumber(L, log10(luaL_check_number(L, 1)));
  return 1;
}

static int math_exp (lua_State *L) {
  lua_pushnumber(L, exp(luaL_check_number(L, 1)));
  return 1;
}

static int math_deg (lua_State *L) {
  lua_pushnumber(L, luaL_check_number(L, 1)/RADIANS_PER_DEGREE);
  return 1;
}

static int math_rad (lua_State *L) {
  lua_pushnumber(L, luaL_check_number(L, 1)*RADIANS_PER_DEGREE);
  return 1;
}

static int math_frexp (lua_State *L) {
  int e;
  lua_pushnumber(L, frexp(luaL_check_number(L, 1), &e));
  lua_pushnumber(L, e);
  return 2;
}

static int math_ldexp (lua_State *L) {
  lua_pushnumber(L, ldexp(luaL_check_number(L, 1), luaL_check_int(L, 2)));
  return 1;
}



static int math_min (lua_State *L) {
  int n = lua_gettop(L);  /* number of arguments */
  lua_Number dmin = luaL_check_number(L, 1);
  int i;
  for (i=2; i<=n; i++) {
    lua_Number d = luaL_check_number(L, i);
    if (d < dmin)
      dmin = d;
  }
  lua_pushnumber(L, dmin);
  return 1;
}


static int math_max (lua_State *L) {
  int n = lua_gettop(L);  /* number of arguments */
  lua_Number dmax = luaL_check_number(L, 1);
  int i;
  for (i=2; i<=n; i++) {
    lua_Number d = luaL_check_number(L, i);
    if (d > dmax)
      dmax = d;
  }
  lua_pushnumber(L, dmax);
  return 1;
}


static int math_random (lua_State *L) {
  /* the `%' avoids the (rare) case of r==1, and is needed also because on
     some systems (SunOS!) `rand()' may return a value larger than RAND_MAX */
  lua_Number r = (lua_Number)(rand()%RAND_MAX) / (lua_Number)RAND_MAX;
  switch (lua_gettop(L)) {  /* check number of arguments */
    case 0: {  /* no arguments */
      lua_pushnumber(L, r);  /* Number between 0 and 1 */
      break;
    }
    case 1: {  /* only upper limit */
      int u = luaL_check_int(L, 1);
      luaL_arg_check(L, 1<=u, 1, l_s("interval is empty"));
      lua_pushnumber(L, (int)(r*u)+1);  /* integer between 1 and `u' */
      break;
    }
    case 2: {  /* lower and upper limits */
      int l = luaL_check_int(L, 1);
      int u = luaL_check_int(L, 2);
      luaL_arg_check(L, l<=u, 2, l_s("interval is empty"));
      lua_pushnumber(L, (int)(r*(u-l+1))+l);  /* integer between `l' and `u' */
      break;
    }
    default: lua_error(L, l_s("wrong number of arguments"));
  }
  return 1;
}


static int math_randomseed (lua_State *L) {
  srand(luaL_check_int(L, 1));
  return 0;
}


static const luaL_reg mathlib[] = {
{l_s("abs"),   math_abs},
{l_s("sin"),   math_sin},
{l_s("cos"),   math_cos},
{l_s("tan"),   math_tan},
{l_s("asin"),  math_asin},
{l_s("acos"),  math_acos},
{l_s("atan"),  math_atan},
{l_s("atan2"), math_atan2},
{l_s("ceil"),  math_ceil},
{l_s("floor"), math_floor},
{l_s("mod"),   math_mod},
{l_s("frexp"), math_frexp},
{l_s("ldexp"), math_ldexp},
{l_s("sqrt"),  math_sqrt},
{l_s("min"),   math_min},
{l_s("max"),   math_max},
{l_s("log"),   math_log},
{l_s("log10"), math_log10},
{l_s("exp"),   math_exp},
{l_s("deg"),   math_deg},
{l_s("rad"),   math_rad},
{l_s("random"),     math_random},
{l_s("randomseed"), math_randomseed}
};

/*
** Open math library
*/
LUALIB_API int lua_mathlibopen (lua_State *L) {
  luaL_openl(L, mathlib);
  lua_pushcfunction(L, math_pow);
  lua_settagmethod(L, LUA_TNUMBER, l_s("pow"));
  lua_pushnumber(L, PI);
  lua_setglobal(L, l_s("PI"));
  return 0;
}

