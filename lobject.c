/*
** $Id: lobject.c,v 1.50 2000/10/02 20:10:55 roberto Exp roberto $
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



const lua_Type luaO_typearr[] = { /* ORDER LUA_T */
  LUA_TUSERDATA, LUA_TNUMBER, LUA_TSTRING, LUA_TTABLE,
  LUA_TFUNCTION, LUA_TFUNCTION, LUA_TNIL
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
    L->nblocks += (n - L->Mbuffsize)*sizeof(char);
    L->Mbuffsize = n;
  }
  return L->Mbuffer;
}


int luaO_str2d (const char *s, Number *result) {  /* LUA_NUMBER */
  char *endptr;
  Number res = lua_str2number(s, &endptr);
  if (endptr == s) return 0;  /* no conversion */
  while (isspace((unsigned char)*endptr)) endptr++;
  if (*endptr != '\0') return 0;  /* invalid trailing characters? */
  *result = res;
  return 1;
}


/* this function needs to handle only '%d' and '%.XXs' formats */
void luaO_verror (lua_State *L, const char *fmt, ...) {
  va_list argp;
  char buff[600];  /* to hold formated message */
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
