/*
** $Id: lobject.c,v 1.68 2001/02/23 20:30:01 roberto Exp roberto $
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


int luaO_equalObj (const TObject *t1, const TObject *t2) {
  if (ttype(t1) != ttype(t2)) return 0;
  switch (ttype(t1)) {
    case LUA_TNUMBER:
      return nvalue(t1) == nvalue(t2);
    case LUA_TNIL:
      return 1;
    default:  /* all other types are equal if pointers are equal */
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


int luaO_str2d (const l_char *s, lua_Number *result) {  /* LUA_NUMBER */
  l_char *endptr;
  lua_Number res = lua_str2number(s, &endptr);
  if (endptr == s) return 0;  /* no conversion */
  while (isspace(uchar(*endptr))) endptr++;
  if (*endptr != l_c('\0')) return 0;  /* invalid trailing characters? */
  *result = res;
  return 1;
}


/* maximum length of a string format for `luaO_verror' */
#define MAX_VERROR	280

/* this function needs to handle only '%d' and '%.XXs' formats */
void luaO_verror (lua_State *L, const l_char *fmt, ...) {
  va_list argp;
  l_char buff[MAX_VERROR];  /* to hold formatted message */
  va_start(argp, fmt);
  vsprintf(buff, fmt, argp);
  va_end(argp);
  luaD_error(L, buff);
}


void luaO_chunkid (l_char *out, const l_char *source, int bufflen) {
  if (*source == l_c('=')) {
    strncpy(out, source+1, bufflen);  /* remove first char */
    out[bufflen-1] = l_c('\0');  /* ensures null termination */
  }
  else {
    if (*source == l_c('@')) {
      int l;
      source++;  /* skip the `@' */
      bufflen -= sizeof(l_s("file `...%s'"));
      l = strlen(source);
      if (l>bufflen) {
        source += (l-bufflen);  /* get last part of file name */
        sprintf(out, l_s("file `...%.99s'"), source);
      }
      else
        sprintf(out, l_s("file `%.99s'"), source);
    }
    else {
      int len = strcspn(source, l_s("\n"));  /* stop at first newline */
      bufflen -= sizeof(l_s("string \"%.*s...\""));
      if (len > bufflen) len = bufflen;
      if (source[len] != l_c('\0')) {  /* must truncate? */
        strcpy(out, l_s("string \""));
        out += strlen(out);
        strncpy(out, source, len);
        strcpy(out+len, l_s("...\""));
      }
      else
        sprintf(out, l_s("string \"%.99s\""), source);
    }
  }
}
