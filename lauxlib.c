/*
** $Id: lauxlib.c,v 1.1 1997/09/16 19:25:59 roberto Exp roberto $
** Auxiliar functions for building Lua libraries
** See Copyright Notice in lua.h
*/


#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "lauxlib.h"
#include "lua.h"
#include "luadebug.h"



void luaL_arg_check(int cond, int numarg, char *extramsg)
{
  if (!cond) {
    char *funcname;
    lua_getobjname(lua_stackedfunction(0), &funcname);
    if (funcname == NULL)
      funcname = "???";
    if (extramsg == NULL)
      luaL_verror("bad argument #%d to function `%.50s'", numarg, funcname);
    else
      luaL_verror("bad argument #%d to function `%.50s' (%.100s)",
                      numarg, funcname, extramsg);
  }
}

char *luaL_check_string (int numArg)
{
  lua_Object o = lua_getparam(numArg);
  luaL_arg_check(lua_isstring(o), numArg, "string expected");
  return lua_getstring(o);
}

char *luaL_opt_string (int numArg, char *def)
{
  return (lua_getparam(numArg) == LUA_NOOBJECT) ? def :
                              luaL_check_string(numArg);
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

lua_Object luaL_nonnullarg (int numArg)
{
  lua_Object o = lua_getparam(numArg);
  luaL_arg_check(o != LUA_NOOBJECT, numArg, "value expected");
  return o;
}

void luaL_openlib (struct luaL_reg *l, int n)
{
  int i;
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


