/*
** $Id: auxlib.h,v 1.1 1997/03/18 15:30:50 roberto Exp $
*/

#ifndef auxlib_h
#define auxlib_h

#include "lua.h"

struct luaL_reg {
  char *name;
  lua_CFunction func;
};

void luaL_openlib (struct luaL_reg *l, int n);
void luaL_arg_check(int cond, int numarg, char *extramsg);
char *luaL_check_string (int numArg);
char *luaL_opt_string (int numArg, char *def);
double luaL_check_number (int numArg);
double luaL_opt_number (int numArg, double def);
void luaL_verror (char *fmt, ...);

#endif
