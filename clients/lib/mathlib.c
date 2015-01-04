/*
** mathlib.c
** Mathematics library to LUA
*/

char *rcs_mathlib="$Id: mathlib.c,v 1.1 1993/12/17 18:41:19 celes Exp $";

#include <stdio.h>		/* NULL */
#include <math.h>

#include "lua.h"

#define TODEGREE(a) ((a)*180.0/3.14159)
#define TORAD(a)    ((a)*3.14159/180.0)

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

static void math_pow (void)
{
 double d1, d2;
 lua_Object o1 = lua_getparam (1);
 lua_Object o2 = lua_getparam (2);
 if (!lua_isnumber(o1) || !lua_isnumber(o2))
 { lua_error ("incorrect arguments to function `pow'"); return; }
 d1 = lua_getnumber(o1);
 d2 = lua_getnumber(o2);
 lua_pushnumber (pow(d1,d2));
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
 lua_register ("pow",   math_pow);
 lua_register ("min",   math_min);
 lua_register ("max",   math_max);
}
