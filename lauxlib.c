/*
** $Id: lauxlib.c,v 1.30 2000/08/09 19:16:57 roberto Exp roberto $
** Auxiliary functions for building Lua libraries
** See Copyright Notice in lua.h
*/


#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* This file uses only the official API of Lua.
** Any function declared here could be written as an application function.
** With care, these functions can be used by other libraries.
*/

#include "lua.h"

#include "lauxlib.h"
#include "luadebug.h"



int luaL_findstring (const char *name, const char *const list[]) {
  int i;
  for (i=0; list[i]; i++)
    if (strcmp(list[i], name) == 0)
      return i;
  return -1;  /* name not found */
}

void luaL_argerror (lua_State *L, int narg, const char *extramsg) {
  lua_Debug ar;
  lua_getstack(L, 0, &ar);
  lua_getinfo(L, "nu", &ar);
  narg -= ar.nups;
  if (ar.name == NULL)
    ar.name = "?";
  luaL_verror(L, "bad argument #%d to `%.50s' (%.100s)",
              narg, ar.name, extramsg);
}


static void type_error (lua_State *L, int narg, const char *type_name) {
  char buff[100];
  const char *rt = lua_type(L, narg);
  if (*rt == 'N') rt = "no value";
  sprintf(buff, "%.10s expected, got %.10s", type_name, rt);
  luaL_argerror(L, narg, buff);
}


/*
** use the 3rd letter of type names for testing:
** nuMber, niL, stRing, fuNction, usErdata, taBle, anY
*/
void luaL_checktype(lua_State *L, int narg, const char *tname) {
  const char *rt = lua_type(L, narg);
  if (!(*rt != 'N' && (tname[2] == 'y' || tname[2] == rt[2])))
    type_error(L, narg, tname);
}


static const char *checkstr (lua_State *L, int narg, size_t *len) {
  const char *s = lua_tostring(L, narg);
  if (!s) type_error(L, narg, "string");
  if (len) *len = lua_strlen(L, narg);
  return s;
}

const char *luaL_check_lstr (lua_State *L, int narg, size_t *len) {
  return checkstr(L, narg, len);
}

const char *luaL_opt_lstr (lua_State *L, int narg, const char *def,
                           size_t *len) {
  if (lua_isnull(L, narg)) {
    if (len) *len = def ? strlen(def) : 0;
    return def;
  }
  else return checkstr(L, narg, len);
}

double luaL_check_number (lua_State *L, int narg) {
  if (!lua_isnumber(L, narg)) type_error(L, narg, "number");
  return lua_tonumber(L, narg);
}


double luaL_opt_number (lua_State *L, int narg, double def) {
  if (lua_isnull(L, narg)) return def;
  else {
    if (!lua_isnumber(L, narg)) type_error(L, narg, "number");
    return lua_tonumber(L, narg);
  }
}


void luaL_openlib (lua_State *L, const struct luaL_reg *l, int n) {
  int i;
  for (i=0; i<n; i++)
    lua_register(L, l[i].name, l[i].func);
}


void luaL_verror (lua_State *L, const char *fmt, ...) {
  char buff[500];
  va_list argp;
  va_start(argp, fmt);
  vsprintf(buff, fmt, argp);
  va_end(argp);
  lua_error(L, buff);
}


#define EXTRALEN	sizeof("string \"...\"0")

void luaL_chunkid (char *out, const char *source, int len) {
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


void luaL_filesource (char *out, const char *filename, int len) {
  if (filename == NULL) filename = "(stdin)";
  sprintf(out, "@%.*s", len-2, filename);  /* -2 for '@' and '\0' */
}

