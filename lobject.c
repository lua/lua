/*
** $Id: lobject.c,v 1.76 2002/04/05 18:54:31 roberto Exp roberto $
** Some generic functions over Lua objects
** See Copyright Notice in lua.h
*/

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"

#include "ldo.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"



const TObject luaO_nilobject = {LUA_TNIL, {NULL}};


int luaO_log2 (unsigned int x) {
  static const lu_byte log_8[255] = {
    0,
    1,1,
    2,2,2,2,
    3,3,3,3,3,3,3,3,
    4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
    5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7
  };
  if (x >= 0x00010000) {
    if (x >= 0x01000000) return log_8[((x>>24) & 0xff) - 1]+24;
    else return log_8[((x>>16) & 0xff) - 1]+16;
  }
  else {
    if (x >= 0x00000100) return log_8[((x>>8) & 0xff) - 1]+8;
    else if (x) return log_8[(x & 0xff) - 1];
    return -1;  /* special `log' for 0 */
  }
}



int luaO_equalObj (const TObject *t1, const TObject *t2) {
  if (ttype(t1) != ttype(t2)) return 0;
  switch (ttype(t1)) {
    case LUA_TNUMBER:
      return nvalue(t1) == nvalue(t2);
    case LUA_TNIL:
      return 1;
    case LUA_TBOOLEAN:
      return bvalue(t1) == bvalue(t2);  /* true must be 1 !! */
    case LUA_TUDATAVAL:
      return pvalue(t1) == pvalue(t2);
    default:  /* other types are equal if struct pointers are equal */
      return tsvalue(t1) == tsvalue(t2);
  }
}


void *luaO_openspaceaux (lua_State *L, size_t n) {
  if (n > G(L)->Mbuffsize) {
    G(L)->Mbuffer = luaM_realloc(L, G(L)->Mbuffer, G(L)->Mbuffsize, n);
    G(L)->Mbuffsize = n;
  }
  return G(L)->Mbuffer;
}


int luaO_str2d (const char *s, lua_Number *result) {
  char *endptr;
  lua_Number res = lua_str2number(s, &endptr);
  if (endptr == s) return 0;  /* no conversion */
  while (isspace((unsigned char)(*endptr))) endptr++;
  if (*endptr != '\0') return 0;  /* invalid trailing characters? */
  *result = res;
  return 1;
}


/* maximum length of a string format for `luaO_verror' */
#define MAX_VERROR	280

/* this function needs to handle only '%d' and '%.XXs' formats */
void luaO_verror (lua_State *L, const char *fmt, ...) {
  va_list argp;
  char buff[MAX_VERROR];  /* to hold formatted message */
  va_start(argp, fmt);
  vsprintf(buff, fmt, argp);
  va_end(argp);
  luaD_runerror(L, buff);
}


void luaO_chunkid (char *out, const char *source, int bufflen) {
  if (*source == '=') {
    strncpy(out, source+1, bufflen);  /* remove first char */
    out[bufflen-1] = '\0';  /* ensures null termination */
  }
  else {
    if (*source == '@') {
      int l;
      source++;  /* skip the `@' */
      bufflen -= sizeof("file `...%s'");
      l = strlen(source);
      if (l>bufflen) {
        source += (l-bufflen);  /* get last part of file name */
        sprintf(out, "file `...%.99s'", source);
      }
      else
        sprintf(out, "file `%.99s'", source);
    }
    else {
      int len = strcspn(source, "\n");  /* stop at first newline */
      bufflen -= sizeof("string \"%.*s...\"");
      if (len > bufflen) len = bufflen;
      if (source[len] != '\0') {  /* must truncate? */
        strcpy(out, "string \"");
        out += strlen(out);
        strncpy(out, source, len);
        strcpy(out+len, "...\"");
      }
      else
        sprintf(out, "string \"%.99s\"", source);
    }
  }
}
