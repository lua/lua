char *rcs_auxlib="$Id: $";

#include <stdio.h>

#include "lua.h"


void luaL_arg_check(int cond, char *funcname, int numarg, char *extramsg)
{
  if (!cond) {
    char buff[100];
    if (extramsg == NULL)
      sprintf(buff, "bad argument #%d to function `%s'", numarg, funcname);
    else
      sprintf(buff, "bad argument #%d to function `%s' (%s)",
                      numarg, funcname, extramsg);
    lua_error(buff);
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

