/*
** mathlib.c
** Mathematics library to LUA
*/

char *rcs_mathlib="$Id: mathlib.c,v 1.25 1997/06/19 18:03:04 roberto Exp $";

#include <stdlib.h>
#include <math.h>

#include "lualib.h"
#include "auxlib.h"
#include "lua.h"

#ifndef PI
#define PI          3.14159265358979323846
#endif
#define TODEGREE(a) ((a)*180.0/PI)
#define TORAD(a)    ((a)*PI/180.0)

static void math_abs (void)
{
 double d = luaL_check_number(1); 
 if (d < 0) d = -d;
 lua_pushnumber (d);
}


static void math_sin (void)
{
 double d = luaL_check_number(1);
 lua_pushnumber (sin(TORAD(d)));
}



static void math_cos (void)
{
 double d = luaL_check_number(1);
 lua_pushnumber (cos(TORAD(d)));
}



static void math_tan (void)
{
 double d = luaL_check_number(1);
 lua_pushnumber (tan(TORAD(d)));
}


static void math_asin (void)
{
 double d = luaL_check_number(1);
 lua_pushnumber (TODEGREE(asin(d)));
}


static void math_acos (void)
{
 double d = luaL_check_number(1);
 lua_pushnumber (TODEGREE(acos(d)));
}


static void math_atan (void)
{
 double d = luaL_check_number(1);
 lua_pushnumber (TODEGREE(atan(d)));
}


static void math_atan2 (void)
{
 double d1 = luaL_check_number(1);
 double d2 = luaL_check_number(2);
 lua_pushnumber (TODEGREE(atan2(d1, d2)));
}


static void math_ceil (void)
{
 double d = luaL_check_number(1);
 lua_pushnumber (ceil(d));
}


static void math_floor (void)
{
 double d = luaL_check_number(1);
 lua_pushnumber (floor(d));
}

static void math_mod (void)
{
  float x = luaL_check_number(1);
  float y = luaL_check_number(2);
  lua_pushnumber(fmod(x, y));
}


static void math_sqrt (void)
{
 double d = luaL_check_number(1);
 lua_pushnumber (sqrt(d));
}


static void math_pow (void)
{
  double d1 = luaL_check_number(1);
  double d2 = luaL_check_number(2);
  lua_pushnumber(pow(d1,d2));
}

static void math_min (void)
{
 int i=1;
 double dmin = luaL_check_number(i);
 while (lua_getparam(++i) != LUA_NOOBJECT)
 {
  double d = luaL_check_number(i);
  if (d < dmin) dmin = d;
 }
 lua_pushnumber (dmin);
}

static void math_max (void)
{
 int i=1;
 double dmax = luaL_check_number(i);
 while (lua_getparam(++i) != LUA_NOOBJECT)
 {
  double d = luaL_check_number(i);
  if (d > dmax) dmax = d;
 }
 lua_pushnumber (dmax);
}

static void math_log (void)
{
 double d = luaL_check_number(1);
 lua_pushnumber (log(d));
}


static void math_log10 (void)
{
 double d = luaL_check_number(1);
 lua_pushnumber (log10(d));
}


static void math_exp (void)
{
 double d = luaL_check_number(1);
 lua_pushnumber (exp(d));
}

static void math_deg (void)
{
 float d = luaL_check_number(1);
 lua_pushnumber (d*180./PI);
}

static void math_rad (void)
{
 float d = luaL_check_number(1);
 lua_pushnumber (d/180.*PI);
}

static void math_random (void)
{
  lua_pushnumber((double)(rand()%RAND_MAX) / (double)RAND_MAX);
}

static void math_randomseed (void)
{
  srand(luaL_check_number(1));
}


static struct luaL_reg mathlib[] = {
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
void mathlib_open (void)
{
  luaL_openlib(mathlib, (sizeof(mathlib)/sizeof(mathlib[0])));
  lua_pushcfunction(math_pow);
  lua_pushnumber(0);  /* to get its tag */
  lua_settagmethod(lua_tag(lua_pop()), "pow");
}

