/*
** $Id: lobject.c,v 1.47 2000/09/11 20:29:27 roberto Exp roberto $
** Some generic functions over Lua objects
** See Copyright Notice in lua.h
*/

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"

#include "lmem.h"
#include "lobject.h"
#include "lstate.h"


/*
** you can use the fact that the 3rd letter or each name is always different
** (e-m-r-b-n-l) to compare and switch these strings
*/
const char *const luaO_typenames[] = { /* ORDER LUA_T */
    "userdata", "number", "string", "table", "function", "function", "nil",
    "function", "function"
};


const TObject luaO_nilobject = {TAG_NIL, {NULL}};


/*
** returns smaller power of 2 larger than `n' (minimum is MINPOWER2) 
*/
lint32 luaO_power2 (lint32 n) {
  lint32 p = MINPOWER2;
  while (p<=n) p<<=1;
  return p;
}


int luaO_equalObj (const TObject *t1, const TObject *t2) {
  if (ttype(t1) != ttype(t2)) return 0;
  switch (ttype(t1)) {
    case TAG_NUMBER:
      return nvalue(t1) == nvalue(t2);
    case TAG_STRING: case TAG_USERDATA:
      return tsvalue(t1) == tsvalue(t2);
    case TAG_TABLE: 
      return hvalue(t1) == hvalue(t2);
    case TAG_CCLOSURE: case TAG_LCLOSURE:
      return clvalue(t1) == clvalue(t2);
    default:
      LUA_ASSERT(ttype(t1) == TAG_NIL, "invalid type");
      return 1; /* TAG_NIL */
  }
}


char *luaO_openspace (lua_State *L, size_t n) {
  if (n > L->Mbuffsize) {
    luaM_reallocvector(L, L->Mbuffer, n, char);
    L->Mbuffsize = n;
  }
  return L->Mbuffer;
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


/* this function needs to handle only '%d' and '%.XXXs' formats */
void luaO_verror (lua_State *L, const char *fmt, ...) {
  char buff[500];
  va_list argp;
  va_start(argp, fmt);
  vsprintf(buff, fmt, argp);
  va_end(argp);
  lua_error(L, buff);
}


#define EXTRALEN	sizeof("string \"...\"0")

void luaO_chunkid (char *out, const char *source, int len) {
  if (*source == '(') {
    strncpy(out, source+1, len-1);  /* remove first char */
    out[len-1] = '\0';  /* make sure `out' has an end */
    out[strlen(out)-1] = '\0';  /* remove last char */
  }
  else {
    len -= EXTRALEN;
    if (*source == '@')
      sprintf(out, "file `%.*s'", len, source+1);
    else {
      const char *b = strchr(source , '\n');  /* stop at first new line */
      int lim = (b && (b-source)<len) ? b-source : len;
      sprintf(out, "string \"%.*s\"", lim, source);
      strcpy(out+lim+(EXTRALEN-sizeof("...\"0")), "...\"");
    }
  }
}
