/*
** $Id: lauxlib.c,v 1.22 1999/12/20 13:09:45 roberto Exp roberto $
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

#define LUA_REENTRANT

#include "lauxlib.h"
#include "lua.h"
#include "luadebug.h"



int luaL_findstring (const char *name, const char *const list[]) {
  int i;
  for (i=0; list[i]; i++)
    if (strcmp(list[i], name) == 0)
      return i;
  return -1;  /* name not found */
}

void luaL_argerror (lua_State *L, int numarg, const char *extramsg) {
  lua_Function f = lua_stackedfunction(L, 0);
  const char *funcname;
  lua_getobjname(L, f, &funcname);
  numarg -= lua_nups(L, f);
  if (funcname == NULL)
    funcname = "?";
  luaL_verror(L, "bad argument #%d to function `%.50s' (%.100s)",
              numarg, funcname, extramsg);
}

static const char *checkstr (lua_State *L, lua_Object o, int n, long *len) {
  const char *s = lua_getstring(L, o);
  luaL_arg_check(L, s, n, "string expected");
  if (len) *len = lua_strlen(L, o);
  return s;
}

const char *luaL_check_lstr (lua_State *L, int n, long *len) {
  return checkstr(L, lua_getparam(L, n), n, len);
}

const char *luaL_opt_lstr (lua_State *L, int n, const char *def, long *len) {
  lua_Object o = lua_getparam(L, n);
  if (o == LUA_NOOBJECT) {
    if (len) *len = def ? strlen(def) : 0;
    return def;
  }
  else return checkstr(L, o, n, len);
}

double luaL_check_number (lua_State *L, int n) {
  lua_Object o = lua_getparam(L, n);
  luaL_arg_check(L, lua_isnumber(L, o), n, "number expected");
  return lua_getnumber(L, o);
}


double luaL_opt_number (lua_State *L, int n, double def) {
  lua_Object o = lua_getparam(L, n);
  if (o == LUA_NOOBJECT) return def;
  else {
    luaL_arg_check(L, lua_isnumber(L, o), n, "number expected");
    return lua_getnumber(L, o);
  }
}


lua_Object luaL_tablearg (lua_State *L, int arg) {
  lua_Object o = lua_getparam(L, arg);
  luaL_arg_check(L, lua_istable(L, o), arg, "table expected");
  return o;
}

lua_Object luaL_functionarg (lua_State *L, int arg) {
  lua_Object o = lua_getparam(L, arg);
  luaL_arg_check(L, lua_isfunction(L, o), arg, "function expected");
  return o;
}

lua_Object luaL_nonnullarg (lua_State *L, int n) {
  lua_Object o = lua_getparam(L, n);
  luaL_arg_check(L, o != LUA_NOOBJECT, n, "value expected");
  return o;
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
