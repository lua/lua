/*
** Libraries to be used in LUA programs
** Grupo de Tecnologia em Computacao Grafica
** TeCGraf - PUC-Rio
** $Id: lualib.h,v 1.4 1995/11/10 17:54:31 roberto Exp $
*/

#ifndef lualib_h
#define lualib_h

void iolib_open   (void);
void strlib_open  (void);
void mathlib_open (void);


/* auxiliar functions (private) */
void lua_arg_error(char *funcname);
char *lua_check_string (int numArg, char *funcname);
float lua_check_number (int numArg, char *funcname);

#endif

