/*
** Libraries to be used in LUA programs
** Grupo de Tecnologia em Computacao Grafica
** TeCGraf - PUC-Rio
** $Id: lualib.h,v 1.6 1996/02/09 19:00:23 roberto Exp roberto $
*/

#ifndef lualib_h
#define lualib_h

void iolib_open   (void);
void strlib_open  (void);
void mathlib_open (void);


/* auxiliar functions (private) */
void lua_arg_error(char *funcname);
char *lua_check_string (int numArg, char *funcname);
double lua_check_number (int numArg, char *funcname);
char *luaI_addchar (int c);
void luaI_addquoted (char *s);

#endif

