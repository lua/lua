/*
** Libraries to be used in LUA programs
** Grupo de Tecnologia em Computacao Grafica
** TeCGraf - PUC-Rio
** $Id: lualib.h,v 1.8 1996/04/30 21:13:55 roberto Exp $
*/

#ifndef lualib_h
#define lualib_h

#include "lua.h"

void iolib_open   (void);
void strlib_open  (void);
void mathlib_open (void);


/* auxiliar functions (private) */

struct lua_reg {
  char *name;
  lua_CFunction func;
};

void luaI_openlib (struct lua_reg *l, int n);
void lua_arg_error(char *funcname);
char *lua_check_string (int numArg, char *funcname);
double lua_check_number (int numArg, char *funcname);
char *luaI_addchar (int c);
void luaI_addquoted (char *s);

#endif

