char *rcs_auxlib="$Id: auxlib.c,v 1.5 1997/04/14 15:30:03 roberto Exp $";

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "lua.h"
#include "auxlib.h"
#include "luadebug.h"



int luaI_findstring (char *name, char *list[])
{
  int i;
  for (i=0; list[i]; i++)
    if (strcmp(list[i], name) == 0)
      return i;
  return -1;  /* name not found */
}


void luaL_arg_check(int cond, int numarg, char *extramsg)
{
  if (!cond) {
    char *funcname;
    lua_getobjname(lua_stackedfunction(0), &funcname);
    if (funcname == NULL)
      funcname = "???";
    if (extramsg == NULL)
      luaL_verror("bad argument #%d to function `%s'", numarg, funcname);
    else
      luaL_verror("bad argument #%d to function `%s' (%s)",
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

void luaL_openlib (struct luaL_reg *l, int n)
{
  int i;
  for (i=0; i<n; i++)
    lua_register(l[i].name, l[i].func);
}


void luaL_verror (char *fmt, ...)
{
  char buff[1000];
  va_list argp;
  va_start(argp, fmt);
  vsprintf(buff, fmt, argp);
  va_end(argp);
  lua_error(buff);
}
