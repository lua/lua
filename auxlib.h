/*
** $Id: $
*/

#ifndef auxlib_h
#define auxlib_h

#include "lua.h"

struct luaL_reg {
  char *name;
  lua_CFunction func;
};

void luaL_openlib (struct luaL_reg *l, int n);
void luaL_arg_check(int cond, char *funcname, int numarg, char *extramsg);
char *luaL_check_string (int numArg, char *funcname);
char *luaL_opt_string (int numArg, char *def, char *funcname);
double luaL_check_number (int numArg, char *funcname);
double luaL_opt_number (int numArg, double def, char *funcname);
void luaL_verror (char *fmt, ...);

#endif
