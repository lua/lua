/*
** $Id: lmathlib.c,v 1.1 1997/09/16 19:25:59 roberto Exp roberto $
** Lua standard mathematical library
** See Copyright Notice in lua.h
*/


#include <stdlib.h>
#include <math.h>

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#ifndef PI
#define PI          3.14159265358979323846
#endif


static double torad = PI/180.0;

#define FROMRAD(a) ((a)/torad)
#define TORAD(a)    ((a)*torad)


static void modeang (void)
{
  char *s = luaL_opt_string(1, "degree");
  switch (*s) {
    case 'd' : torad = PI/180.0; break;
    case 'r' : torad = 1.0; break;
    case 'g' : torad = PI/50.0; break;
    default: luaL_arg_check(0, 1, "invalid mode");
  }
}


static void math_abs (void)
{
  double d = luaL_check_number(1);
  if (d < 0) d = -d;
  lua_pushnumber(d);
}

static void math_sin (void)
{
  lua_pushnumber(sin(TORAD(luaL_check_number(1))));
}

static void math_cos (void)
{
  lua_pushnumber(cos(TORAD(luaL_check_number(1))));
}

static void math_tan (void)
{
  lua_pushnumber(tan(TORAD(luaL_check_number(1))));
}

static void math_asin (void)
{
  lua_pushnumber(FROMRAD(asin(luaL_check_number(1))));
}

static void math_acos (void)
{
  lua_pushnumber(FROMRAD(acos(luaL_check_number(1))));
}

static void math_atan (void)
{
  lua_pushnumber(FROMRAD(atan(luaL_check_number(1))));
}

static void math_atan2 (void)
{
  lua_pushnumber(FROMRAD(atan2(luaL_check_number(1), luaL_check_number(2))));
}

static void math_ceil (void)
{
  lua_pushnumber(ceil(luaL_check_number(1)));
}

static void math_floor (void)
{
  lua_pushnumber(floor(luaL_check_number(1)));
}

static void math_mod (void)
{
  lua_pushnumber(fmod(luaL_check_number(1), luaL_check_number(2)));
}

static void math_sqrt (void)
{
  lua_pushnumber(sqrt(luaL_check_number(1)));
}

static void math_pow (void)
{
  lua_pushnumber(pow(luaL_check_number(1), luaL_check_number(2)));
}

static void math_log (void)
{
  lua_pushnumber(log(luaL_check_number(1)));
}

static void math_log10 (void)
{
  lua_pushnumber(log10(luaL_check_number(1)));
}

static void math_exp (void)
{
  lua_pushnumber(exp(luaL_check_number(1)));
}

static void math_deg (void)
{
  lua_pushnumber(luaL_check_number(1)*180./PI);
}

static void math_rad (void)
{
  lua_pushnumber(luaL_check_number(1)/180.*PI);
}



static void math_min (void)
{
  int i = 1;
  double dmin = luaL_check_number(i);
  while (lua_getparam(++i) != LUA_NOOBJECT) {
    double d = luaL_check_number(i);
    if (d < dmin)
      dmin = d;
  }
  lua_pushnumber(dmin);
}


static void math_max (void)
{
  int i = 1;
  double dmax = luaL_check_number(i);
  while (lua_getparam(++i) != LUA_NOOBJECT) {
    double d = luaL_check_number(i);
    if (d > dmax)
      dmax = d;
  }
  lua_pushnumber(dmax);
}


static void math_random (void)
{
  double l = luaL_opt_number(1, 0);
  double r = (double)rand() / (double)RAND_MAX;
  if (l == 0)
    lua_pushnumber(r);
  else
    lua_pushnumber((int)(r*l)+1);
}


static void math_randomseed (void)
{
  srand(luaL_check_number(1));
}


static struct luaL_reg mathlib[] = {
{"modeang",   modeang},
{"abs",   math_abs},
{"sin",   math_sin},
{"cos",   math_cos},
{"tan",   math_tan},
{"asin",  math_asin},
{"acos",  math_acos},
{"atan",  math_atan},
{"atan2",  math_atan2},
{"ceil",  math_ceil},
{"floor", math_floor},
{"mod",   math_mod},
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
void lua_mathlibopen (void)
{
  luaL_openlib(mathlib, (sizeof(mathlib)/sizeof(mathlib[0])));
  lua_pushcfunction(math_pow);
  lua_pushnumber(0);  /* to get its tag */
  lua_settagmethod(lua_tag(lua_pop()), "pow");
  lua_pushnumber(PI); lua_setglobal("PI");
}

