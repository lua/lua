/*
** $Id: lobject.c,v 1.32 2000/03/03 14:58:26 roberto Exp roberto $
** Some generic functions over Lua objects
** See Copyright Notice in lua.h
*/

#include <ctype.h>
#include <stdlib.h>

#define LUA_REENTRANT

#include "lobject.h"
#include "lua.h"


const char *const luaO_typenames[] = { /* ORDER LUA_T */
    "userdata", "number", "string", "table", "function", "function", "nil",
    "function", "function", "function", "function", "function", "function",
    "line", NULL
};


const TObject luaO_nilobject = {TAG_NIL, {NULL}};


/*
** returns smaller power of 2 larger than `n' (minimum is MINPOWER2) 
*/
unsigned long luaO_power2 (unsigned long n) {
  unsigned long p = MINPOWER2;
  while (p<=n) p<<=1;
  return p;
}


int luaO_equalval (const TObject *t1, const TObject *t2) {
  switch (ttype(t1)) {
    case TAG_NIL:
      return 1;
    case TAG_NUMBER:
      return nvalue(t1) == nvalue(t2);
    case TAG_STRING: case TAG_USERDATA:
      return svalue(t1) == svalue(t2);
    case TAG_ARRAY: 
      return avalue(t1) == avalue(t2);
    case TAG_LPROTO:
      return tfvalue(t1)  == tfvalue(t2);
    case TAG_CPROTO:
      return fvalue(t1)  == fvalue(t2);
    case TAG_CCLOSURE: case TAG_LCLOSURE:
      return t1->value.cl == t2->value.cl;
    default:
     LUA_INTERNALERROR(L, "invalid type");
     return 0; /* UNREACHABLE */
  }
}


static double expten (unsigned int e) {
  double exp = 10.0;
  double res = 1.0;
  for (; e; e>>=1) {
    if (e & 1) res *= exp;
    exp *= exp;
  }
  return res;
}


int luaO_str2d (const char *s, Number *result) {  /* LUA_NUMBER */
  double a = 0.0;
  int point = 0;  /* number of decimal digits */
  int sig;
  while (isspace((unsigned char)*s)) s++;
  sig = 0;
  switch (*s) {
    case '-': sig = 1;  /* go through */
    case '+': s++;
  }
  if (! (isdigit((unsigned char)*s) ||
          (*s == '.' && isdigit((unsigned char)*(s+1)))))
    return 0;  /* not (at least one digit before or after the point) */
  while (isdigit((unsigned char)*s))
    a = 10.0*a + (*(s++)-'0');
  if (*s == '.') {
    s++;
    while (isdigit((unsigned char)*s)) {
      a = 10.0*a + (*(s++)-'0');
      point++;
    }
  }
  if (sig) a = -a;
  if (*s == 'e' || *s == 'E') {
    int e = 0;
    s++;
    sig = 0;
    switch (*s) {
      case '-': sig = 1;  /* go through */
      case '+': s++;
    }
    if (!isdigit((unsigned char)*s)) return 0;  /* no digit in the exponent? */
    do {
      e = 10*e + (*(s++)-'0');
    } while (isdigit((unsigned char)*s));
    if (sig) e = -e;
    point -= e;
  }
  while (isspace((unsigned char)*s)) s++;
  if (*s != '\0') return 0;  /* invalid trailing characters? */
  if (point != 0) {
    if (point > 0) a /= expten(point);
    else           a *= expten(-point);
  }
  *result = a;
  return 1;
}

