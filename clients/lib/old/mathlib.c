/*
** mathlib.c
** Mathematics library to LUA
*/

char *rcs_mathlib="$Id: mathlib.c,v 1.17 1996/04/30 21:13:55 roberto Exp $";

#include <stdlib.h>
#include <math.h>

#include "lualib.h"
#include "lua.h"

#ifndef PI
#define PI          3.14159265358979323846
#endif
#define TODEGREE(a) ((a)*180.0/PI)
#define TORAD(a)    ((a)*PI/180.0)

static void math_abs (void)
{
 double d = lua_check_number(1, "abs"); 
 if (d < 0) d = -d;
 lua_pushnumber (d);
}


static void math_sin (void)
{
 double d = lua_check_number(1, "sin");
 lua_pushnumber (sin(TORAD(d)));
}



static void math_cos (void)
{
 double d = lua_check_number(1, "cos");
 lua_pushnumber (cos(TORAD(d)));
}



static void math_tan (void)
{
 double d = lua_check_number(1, "tan");
 lua_pushnumber (tan(TORAD(d)));
}


static void math_asin (void)
{
 double d = lua_check_number(1, "asin");
 lua_pushnumber (TODEGREE(asin(d)));
}


static void math_acos (void)
{
 double d = lua_check_number(1, "acos");
 lua_pushnumber (TODEGREE(acos(d)));
}


static void math_atan (void)
{
 double d = lua_check_number(1, "atan");
 lua_pushnumber (TODEGREE(atan(d)));
}


static void math_atan2 (void)
{
 double d1 = lua_check_number(1, "atan2");
 double d2 = lua_check_number(2, "atan2");
 lua_pushnumber (TODEGREE(atan2(d1, d2)));
}


static void math_ceil (void)
{
 double d = lua_check_number(1, "ceil");
 lua_pushnumber (ceil(d));
}


static void math_floor (void)
{
 double d = lua_check_number(1, "floor");
 lua_pushnumber (floor(d));
}

static void math_mod (void)
{
 int d1 = (int)lua_check_number(1, "mod");
 int d2 = (int)lua_check_number(2, "mod");
 lua_pushnumber (d1%d2);
}


static void math_sqrt (void)
{
 double d = lua_check_number(1, "sqrt");
 lua_pushnumber (sqrt(d));
}

static int old_pow;

static void math_pow (void)
{
 lua_Object o1 = lua_getparam (1);
 lua_Object o2 = lua_getparam (2);
 lua_Object op = lua_getparam(3);
 if (!lua_isnumber(o1) || !lua_isnumber(o2) || *(lua_getstring(op)) != 'p')
 {
   lua_Object old = lua_getref(old_pow);
   lua_pushobject(o1);
   lua_pushobject(o2);
   lua_pushobject(op);
   if (lua_callfunction(old) != 0)
     lua_error(NULL);
 }
 else
 {
   double d1 = lua_getnumber(o1);
   double d2 = lua_getnumber(o2);
   lua_pushnumber (pow(d1,d2));
 }
}

static void math_min (void)
{
 int i=1;
 double dmin = lua_check_number(i, "min");
 while (lua_getparam(++i) != LUA_NOOBJECT)
 {
  double d = lua_check_number(i, "min");
  if (d < dmin) dmin = d;
 }
 lua_pushnumber (dmin);
}

static void math_max (void)
{
 int i=1;
 double dmax = lua_check_number(i, "max");
 while (lua_getparam(++i) != LUA_NOOBJECT)
 {
  double d = lua_check_number(i, "max");
  if (d > dmax) dmax = d;
 }
 lua_pushnumber (dmax);
}

static void math_log (void)
{
 double d = lua_check_number(1, "log");
 lua_pushnumber (log(d));
}


static void math_log10 (void)
{
 double d = lua_check_number(1, "log10");
 lua_pushnumber (log10(d));
}


static void math_exp (void)
{
 double d = lua_check_number(1, "exp");
 lua_pushnumber (exp(d));
}

static void math_deg (void)
{
 float d = lua_check_number(1, "deg");
 lua_pushnumber (d*180./PI);
}

static void math_rad (void)
{
 float d = lua_check_number(1, "rad");
 lua_pushnumber (d/180.*PI);
}

static void math_random (void)
{
  lua_pushnumber((double)(rand()%RAND_MAX) / (double)RAND_MAX);
}

static void math_randomseed (void)
{
  srand(lua_check_number(1, "randomseed"));
}


static struct lua_reg mathlib[] = {
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
  luaI_openlib(mathlib, (sizeof(mathlib)/sizeof(mathlib[0])));
  old_pow = lua_refobject(lua_setfallback("arith", math_pow), 1);
}

