/*
** mathlib.c
** Mathematics library to LUA
*/

char *rcs_mathlib="$Id: mathlib.c,v 1.4 1994/10/11 13:06:47 celes Exp roberto $";

#include <stdio.h>		/* NULL */
#include <math.h>

#include "lualib.h"
#include "lua.h"

#define PI          (3.141592653589793)
#define TODEGREE(a) ((a)*180.0/PI)
#define TORAD(a)    ((a)*PI/180.0)

static void math_abs (void)
{
 double d;
 lua_Object o = lua_getparam (1);
 if (o == NULL)
 { lua_error ("too few arguments to function `abs'"); return; }
 if (!lua_isnumber(o))
 { lua_error ("incorrect arguments to function `abs'"); return; }
 d = lua_getnumber(o);
 if (d < 0) d = -d;
 lua_pushnumber (d);
}


static void math_sin (void)
{
 double d;
 lua_Object o = lua_getparam (1);
 if (o == NULL)
 { lua_error ("too few arguments to function `sin'"); return; }
 if (!lua_isnumber(o))
 { lua_error ("incorrect arguments to function `sin'"); return; }
 d = lua_getnumber(o);
 lua_pushnumber (sin(TORAD(d)));
}



static void math_cos (void)
{
 double d;
 lua_Object o = lua_getparam (1);
 if (o == NULL)
 { lua_error ("too few arguments to function `cos'"); return; }
 if (!lua_isnumber(o))
 { lua_error ("incorrect arguments to function `cos'"); return; }
 d = lua_getnumber(o);
 lua_pushnumber (cos(TORAD(d)));
}



static void math_tan (void)
{
 double d;
 lua_Object o = lua_getparam (1);
 if (o == NULL)
 { lua_error ("too few arguments to function `tan'"); return; }
 if (!lua_isnumber(o))
 { lua_error ("incorrect arguments to function `tan'"); return; }
 d = lua_getnumber(o);
 lua_pushnumber (tan(TORAD(d)));
}


static void math_asin (void)
{
 double d;
 lua_Object o = lua_getparam (1);
 if (o == NULL)
 { lua_error ("too few arguments to function `asin'"); return; }
 if (!lua_isnumber(o))
 { lua_error ("incorrect arguments to function `asin'"); return; }
 d = lua_getnumber(o);
 lua_pushnumber (TODEGREE(asin(d)));
}


static void math_acos (void)
{
 double d;
 lua_Object o = lua_getparam (1);
 if (o == NULL)
 { lua_error ("too few arguments to function `acos'"); return; }
 if (!lua_isnumber(o))
 { lua_error ("incorrect arguments to function `acos'"); return; }
 d = lua_getnumber(o);
 lua_pushnumber (TODEGREE(acos(d)));
}



static void math_atan (void)
{
 double d;
 lua_Object o = lua_getparam (1);
 if (o == NULL)
 { lua_error ("too few arguments to function `atan'"); return; }
 if (!lua_isnumber(o))
 { lua_error ("incorrect arguments to function `atan'"); return; }
 d = lua_getnumber(o);
 lua_pushnumber (TODEGREE(atan(d)));
}


static void math_ceil (void)
{
 double d;
 lua_Object o = lua_getparam (1);
 if (o == NULL)
 { lua_error ("too few arguments to function `ceil'"); return; }
 if (!lua_isnumber(o))
 { lua_error ("incorrect arguments to function `ceil'"); return; }
 d = lua_getnumber(o);
 lua_pushnumber (ceil(d));
}


static void math_floor (void)
{
 double d;
 lua_Object o = lua_getparam (1);
 if (o == NULL)
 { lua_error ("too few arguments to function `floor'"); return; }
 if (!lua_isnumber(o))
 { lua_error ("incorrect arguments to function `floor'"); return; }
 d = lua_getnumber(o);
 lua_pushnumber (floor(d));
}

static void math_mod (void)
{
 int d1, d2;
 lua_Object o1 = lua_getparam (1);
 lua_Object o2 = lua_getparam (2);
 if (!lua_isnumber(o1) || !lua_isnumber(o2))
 { lua_error ("incorrect arguments to function `mod'"); return; }
 d1 = (int) lua_getnumber(o1);
 d2 = (int) lua_getnumber(o2);
 lua_pushnumber (d1%d2);
}


static void math_sqrt (void)
{
 double d;
 lua_Object o = lua_getparam (1);
 if (o == NULL)
 { lua_error ("too few arguments to function `sqrt'"); return; }
 if (!lua_isnumber(o))
 { lua_error ("incorrect arguments to function `sqrt'"); return; }
 d = lua_getnumber(o);
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
   lua_pushobject(o1);
   lua_pushobject(o2);
   lua_pushobject(op);
   if (lua_callfunction(lua_getlocked(old_pow)) != 0)
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
 double d, dmin;
 lua_Object o;
 if ((o = lua_getparam(i++)) == NULL)
 { lua_error ("too few arguments to function `min'"); return; }
 if (!lua_isnumber(o))
 { lua_error ("incorrect arguments to function `min'"); return; }
 dmin = lua_getnumber (o);
 while ((o = lua_getparam(i++)) != NULL)
 {
  if (!lua_isnumber(o))
  { lua_error ("incorrect arguments to function `min'"); return; }
  d = lua_getnumber (o);
  if (d < dmin) dmin = d;
 }
 lua_pushnumber (dmin);
}


static void math_max (void)
{
 int i=1;
 double d, dmax;
 lua_Object o;
 if ((o = lua_getparam(i++)) == NULL)
 { lua_error ("too few arguments to function `max'"); return; }
 if (!lua_isnumber(o))
 { lua_error ("incorrect arguments to function `max'"); return; }
 dmax = lua_getnumber (o);
 while ((o = lua_getparam(i++)) != NULL)
 {
  if (!lua_isnumber(o))
  { lua_error ("incorrect arguments to function `max'"); return; }
  d = lua_getnumber (o);
  if (d > dmax) dmax = d;
 }
 lua_pushnumber (dmax);
}


static void math_log (void)
{
 double d;
 lua_Object o = lua_getparam (1);
 if (o == NULL)
 { lua_error ("too few arguments to function `log'"); return; }
 if (!lua_isnumber(o))
 { lua_error ("incorrect arguments to function `log'"); return; }
 d = lua_getnumber(o);
 lua_pushnumber (log(d));
}


static void math_log10 (void)
{
 double d;
 lua_Object o = lua_getparam (1);
 if (o == NULL)
 { lua_error ("too few arguments to function `log10'"); return; }
 if (!lua_isnumber(o))
 { lua_error ("incorrect arguments to function `log10'"); return; }
 d = lua_getnumber(o);
 lua_pushnumber (log10(d));
}


static void math_exp (void)
{
 double d;
 lua_Object o = lua_getparam (1);
 if (o == NULL)
 { lua_error ("too few arguments to function `exp'"); return; }
 if (!lua_isnumber(o))
 { lua_error ("incorrect arguments to function `exp'"); return; }
 d = lua_getnumber(o);
 lua_pushnumber (exp(d));
}

static void math_deg (void)
{
 float d;
 lua_Object o = lua_getparam (1);
 if (o == NULL)
 { lua_error ("too few arguments to function `deg'"); return; }
 if (!lua_isnumber(o))
 { lua_error ("incorrect arguments to function `deg'"); return; }
 d = lua_getnumber(o);
 lua_pushnumber (d*180./PI);
}

static void math_rad (void)
{
 float d;
 lua_Object o = lua_getparam (1);
 if (o == NULL)
 { lua_error ("too few arguments to function `rad'"); return; }
 if (!lua_isnumber(o))
 { lua_error ("incorrect arguments to function `rad'"); return; }
 d = lua_getnumber(o);
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
 old_pow = lua_lock(lua_setfallback("arith", math_pow));
}
