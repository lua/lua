/*
** mathlib.c
** Mathematics library to LUA
*/

char *rcs_mathlib="$Id: mathlib.c,v 1.13 1995/11/10 17:54:31 roberto Exp $";

#include <stdio.h>		/* NULL */
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
   lua_Object old = lua_getlocked(old_pow);
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

/*
** Open math library
*/
void mathlib_open (void)
{
 lua_register ("abs",   math_abs);
 lua_register ("sin",   math_sin);
 lua_register ("cos",   math_cos);
 lua_register ("tan",   math_tan);
 lua_register ("asin",  math_asin);
 lua_register ("acos",  math_acos);
 lua_register ("atan",  math_atan);
 lua_register ("atan2",  math_atan2);
 lua_register ("ceil",  math_ceil);
 lua_register ("floor", math_floor);
 lua_register ("mod",   math_mod);
 lua_register ("sqrt",  math_sqrt);
 lua_register ("min",   math_min);
 lua_register ("max",   math_max);
 lua_register ("log",   math_log);
 lua_register ("log10", math_log10);
 lua_register ("exp",   math_exp);
 lua_register ("deg",   math_deg);
 lua_register ("rad",   math_rad);
 old_pow = lua_lockobject(lua_setfallback("arith", math_pow));
}
