/*
** Libraries to be used in LUA programs
** Grupo de Tecnologia em Computacao Grafica
** TeCGraf - PUC-Rio
** $Id: lualib.h,v 1.3 1994/12/13 15:59:16 roberto Exp roberto $
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

