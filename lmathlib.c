/*
** $Id: lmathlib.c,v 1.24 2000/03/10 18:37:44 roberto Exp roberto $
** Standard mathematical library
** See Copyright Notice in lua.h
*/


#include <stdlib.h>
#include <math.h>

#define LUA_REENTRANT

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


static void math_abs (lua_State *L) {
  lua_pushnumber(L, fabs(luaL_check_number(L, 1)));
}

static void math_sin (lua_State *L) {
  lua_pushnumber(L, sin(TORAD(luaL_check_number(L, 1))));
}

static void math_cos (lua_State *L) {
  lua_pushnumber(L, cos(TORAD(luaL_check_number(L, 1))));
}

static void math_tan (lua_State *L) {
  lua_pushnumber(L, tan(TORAD(luaL_check_number(L, 1))));
}

static void math_asin (lua_State *L) {
  lua_pushnumber(L, FROMRAD(asin(luaL_check_number(L, 1))));
}

static void math_acos (lua_State *L) {
  lua_pushnumber(L, FROMRAD(acos(luaL_check_number(L, 1))));
}

static void math_atan (lua_State *L) {
  lua_pushnumber(L, FROMRAD(atan(luaL_check_number(L, 1))));
}

static void math_atan2 (lua_State *L) {
  lua_pushnumber(L, FROMRAD(atan2(luaL_check_number(L, 1), luaL_check_number(L, 2))));
}

static void math_ceil (lua_State *L) {
  lua_pushnumber(L, ceil(luaL_check_number(L, 1)));
}

static void math_floor (lua_State *L) {
  lua_pushnumber(L, floor(luaL_check_number(L, 1)));
}

static void math_mod (lua_State *L) {
  lua_pushnumber(L, fmod(luaL_check_number(L, 1), luaL_check_number(L, 2)));
}

static void math_sqrt (lua_State *L) {
  lua_pushnumber(L, sqrt(luaL_check_number(L, 1)));
}

static void math_pow (lua_State *L) {
  lua_pushnumber(L, pow(luaL_check_number(L, 1), luaL_check_number(L, 2)));
}

static void math_log (lua_State *L) {
  lua_pushnumber(L, log(luaL_check_number(L, 1)));
}

static void math_log10 (lua_State *L) {
  lua_pushnumber(L, log10(luaL_check_number(L, 1)));
}

static void math_exp (lua_State *L) {
  lua_pushnumber(L, exp(luaL_check_number(L, 1)));
}

static void math_deg (lua_State *L) {
  lua_pushnumber(L, luaL_check_number(L, 1)/RADIANS_PER_DEGREE);
}

static void math_rad (lua_State *L) {
  lua_pushnumber(L, luaL_check_number(L, 1)*RADIANS_PER_DEGREE);
}

static void math_frexp (lua_State *L) {
  int e;
  lua_pushnumber(L, frexp(luaL_check_number(L, 1), &e));
  lua_pushnumber(L, e);
}

static void math_ldexp (lua_State *L) {
  lua_pushnumber(L, ldexp(luaL_check_number(L, 1), luaL_check_int(L, 2)));
}



static void math_min (lua_State *L) {
  int i = 1;
  double dmin = luaL_check_number(L, i);
  while (lua_getparam(L, ++i) != LUA_NOOBJECT) {
    double d = luaL_check_number(L, i);
    if (d < dmin)
      dmin = d;
  }
  lua_pushnumber(L, dmin);
}


static void math_max (lua_State *L) {
  int i = 1;
  double dmax = luaL_check_number(L, i);
  while (lua_getparam(L, ++i) != LUA_NOOBJECT) {
    double d = luaL_check_number(L, i);
    if (d > dmax)
      dmax = d;
  }
  lua_pushnumber(L, dmax);
}


static void math_random (lua_State *L) {
  /* the '%' avoids the (rare) case of r==1, and is needed also because on
     some systems (SunOS!) "rand()" may return a value larger than RAND_MAX */
  double r = (double)(rand()%RAND_MAX) / (double)RAND_MAX;
  if (lua_getparam(L, 1) == LUA_NOOBJECT)  /* no arguments? */
    lua_pushnumber(L, r);  /* Number between 0 and 1 */
  else {
    int l, u;  /* lower & upper limits */
    if (lua_getparam(L, 2) == LUA_NOOBJECT) {  /* only one argument? */
      l = 1;
      u = luaL_check_int(L, 1);
    }
    else {  /* two arguments */
      l = luaL_check_int(L, 1);
      u = luaL_check_int(L, 2);
    }
    luaL_arg_check(L, l<=u, 1, "interval is empty");
    lua_pushnumber(L, (int)(r*(u-l+1))+l);  /* integer between `l' and `u' */
  }
}


static void math_randomseed (lua_State *L) {
  srand(luaL_check_int(L, 1));
}


static const struct luaL_reg mathlib[] = {
{"abs",   math_abs},
{"sin",   math_sin},
{"cos",   math_cos},
{"tan",   math_tan},
{"asin",  math_asin},
{"acos",  math_acos},
{"atan",  math_atan},
{"atan2", math_atan2},
{"ceil",  math_ceil},
{"floor", math_floor},
{"mod",   math_mod},
{"frexp", math_frexp},
{"ldexp", math_ldexp},
{"sqrt",  math_sqrt},
{"min",   math_min},
{"max",   math_max},
{"log",   math_log},
{"log10", math_log10},
{"exp",   math_exp},
{"deg",   math_deg},
{"rad",   math_rad},
{"random",     math_random},
{"randomseed", math_randomseed}
};

/*
** Open math library
*/
void lua_mathlibopen (lua_State *L) {
  luaL_openl(L, mathlib);
  lua_pushcfunction(L, math_pow);
  lua_pushnumber(L, 0);  /* to get its tag */
  lua_settagmethod(L, lua_tag(L, lua_pop(L)), "pow");
  lua_pushnumber(L, PI); lua_setglobal(L, "PI");
}

