/*
** $Id: lauxlib.h,v 1.5 1997/12/17 20:48:58 roberto Exp roberto $
** Auxiliar functions for building Lua libraries
** See Copyright Notice in lua.h
*/


#ifndef auxlib_h
#define auxlib_h


#include "lua.h"


struct luaL_reg {
  char *name;
  lua_CFunction func;
};


#define luaL_arg_check(cond,numarg,extramsg) if (!(cond)) \
                                               luaL_argerror(numarg,extramsg)

void luaL_openlib (struct luaL_reg *l, int n);
void luaL_argerror (int numarg, char *extramsg);
char *luaL_check_string (int numArg);
char *luaL_opt_string (int numArg, char *def);
double luaL_check_number (int numArg);
double luaL_opt_number (int numArg, double def);
lua_Object luaL_functionarg (int arg);
lua_Object luaL_tablearg (int arg);
lua_Object luaL_nonnullarg (int numArg);
void luaL_verror (char *fmt, ...);
char *luaL_openspace (int size);
void luaL_resetbuffer (void);
void luaL_addchar (int c);
void luaL_addsize (int n);
int luaL_newbuffer (int size);
void luaL_oldbuffer (int old);
char *luaL_buffer (void);


#endif
