/*
** Libraries to be used in LUA programs
** Grupo de Tecnologia em Computacao Grafica
** TeCGraf - PUC-Rio
** $Id: lualib.h,v 1.11 1997/03/17 17:01:10 roberto Exp roberto $
*/

#ifndef lualib_h
#define lualib_h

#include "lua.h"

void iolib_open   (void);
void strlib_open  (void);
void mathlib_open (void);



/* auxiliar functions (private) */

char *luaI_addchar (int c);
void luaI_addquoted (char *s);

char *item_end (char *p);
int singlematch (int c, char *p);

#endif

