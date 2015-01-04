/*
** $Id: lauxlib.c,v 1.17 1999/03/11 18:59:19 roberto Exp $
** Auxiliary functions for building Lua libraries
** See Copyright Notice in lua.h
*/


#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* Please Notice: This file uses only the official API of Lua
** Any function declared here could be written as an application function.
** With care, these functions can be used by other libraries.
*/

#include "lauxlib.h"
#include "lua.h"
#include "luadebug.h"



int luaL_findstring (char *name, char *list[]) {
  int i;
  for (i=0; list[i]; i++)
    if (strcmp(list[i], name) == 0)
      return i;
  return -1;  /* name not found */
}

void luaL_argerror (int numarg, char *extramsg) {
  lua_Function f = lua_stackedfunction(0);
  char *funcname;
  lua_getobjname(f, &funcname);
  numarg -= lua_nups(f);
  if (funcname == NULL)
    funcname = "?";
  if (extramsg == NULL)
    luaL_verror("bad argument #%d to function `%.50s'", numarg, funcname);
  else
    luaL_verror("bad argument #%d to function `%.50s' (%.100s)",
                    numarg, funcname, extramsg);
}

char *luaL_check_lstr (int numArg, long *len)
{
  lua_Object o = lua_getparam(numArg);
  luaL_arg_check(lua_isstring(o), numArg, "string expected");
  if (len) *len = lua_strlen(o);
  return lua_getstring(o);
}

char *luaL_opt_lstr (int numArg, char *def, long *len)
{
  return (lua_getparam(numArg) == LUA_NOOBJECT) ? def :
                              luaL_check_lstr(numArg, len);
}

double luaL_check_number (int numArg)
{
  lua_Object o = lua_getparam(numArg);
  luaL_arg_check(lua_isnumber(o), numArg, "number expected");
  return lua_getnumber(o);
}


double luaL_opt_number (int numArg, double def)
{
  return (lua_getparam(numArg) == LUA_NOOBJECT) ? def :
                              luaL_check_number(numArg);
}  


lua_Object luaL_tablearg (int arg)
{
  lua_Object o = lua_getparam(arg);
  luaL_arg_check(lua_istable(o), arg, "table expected");
  return o;
}

lua_Object luaL_functionarg (int arg)
{
  lua_Object o = lua_getparam(arg);
  luaL_arg_check(lua_isfunction(o), arg, "function expected");
  return o;
}

lua_Object luaL_nonnullarg (int numArg)
{
  lua_Object o = lua_getparam(numArg);
  luaL_arg_check(o != LUA_NOOBJECT, numArg, "value expected");
  return o;
}

void luaL_openlib (struct luaL_reg *l, int n)
{
  int i;
  lua_open();  /* make sure lua is already open */
  for (i=0; i<n; i++)
    lua_register(l[i].name, l[i].func);
}


void luaL_verror (char *fmt, ...)
{
  char buff[500];
  va_list argp;
  va_start(argp, fmt);
  vsprintf(buff, fmt, argp);
  va_end(argp);
  lua_error(buff);
}


void luaL_chunkid (char *out, char *source, int len) {
  len -= 13;  /* 13 = strlen("string ''...\0") */
  if (*source == '@')
    sprintf(out, "file `%.*s'", len, source+1);
  else if (*source == '(')
    strcpy(out, "(C code)");
  else {
    char *b = strchr(source , '\n');  /* stop string at first new line */
    int lim = (b && (b-source)<len) ? b-source : len;
    sprintf(out, "string `%.*s'", lim, source);
    strcpy(out+lim+(13-5), "...'");  /* 5 = strlen("...'\0") */
  }
}


void luaL_filesource (char *out, char *filename, int len) {
  if (filename == NULL) filename = "(stdin)";
  sprintf(out, "@%.*s", len-2, filename);  /* -2 for '@' and '\0' */
}
