/*
** Libraries to be used in LUA programs
** Grupo de Tecnologia em Computacao Grafica
** TeCGraf - PUC-Rio
** $Id: lualib.h,v 1.9 1996/08/01 14:55:33 roberto Exp roberto $
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
void lua_arg_check(int cond, char *funcname);
char *lua_check_string (int numArg, char *funcname);
char *lua_opt_string (int numArg, char *def, char *funcname);
double lua_check_number (int numArg, char *funcname);
long lua_opt_number (int numArg, long def, char *funcname);
char *luaI_addchar (int c);
void luaI_addquoted (char *s);

char *item_end (char *p);
int singlematch (int c, char *p);

#endif

