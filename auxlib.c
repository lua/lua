char *rcs_auxlib="$Id: auxlib.c,v 1.1 1997/03/17 17:02:29 roberto Exp roberto $";

#include <stdio.h>
#include <stdarg.h>

#include "lua.h"
#include "auxlib.h"


void luaL_arg_check(int cond, char *funcname, int numarg, char *extramsg)
{
  if (!cond) {
    if (extramsg == NULL)
      luaL_verror("bad argument #%d to function `%s'", numarg, funcname);
    else
      luaL_verror("bad argument #%d to function `%s' (%s)",
                      numarg, funcname, extramsg);
  }
}

char *luaL_check_string (int numArg, char *funcname)
{
  lua_Object o = lua_getparam(numArg);
  luaL_arg_check(lua_isstring(o), funcname, numArg, "string expected");
  return lua_getstring(o);
}

char *luaL_opt_string (int numArg, char *def, char *funcname)
{
  return (lua_getparam(numArg) == LUA_NOOBJECT) ? def :
                              luaL_check_string(numArg, funcname);
}

double luaL_check_number (int numArg, char *funcname)
{
  lua_Object o = lua_getparam(numArg);
  luaL_arg_check(lua_isnumber(o), funcname, numArg, "number expected");
  return lua_getnumber(o);
}


double luaL_opt_number (int numArg, double def, char *funcname)
{
  return (lua_getparam(numArg) == LUA_NOOBJECT) ? def :
                              luaL_check_number(numArg, funcname);
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
