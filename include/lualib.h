/*
** Libraries to be used in LUA programs
** Grupo de Tecnologia em Computacao Grafica
** TeCGraf - PUC-Rio
** $Id: lualib.h,v 1.13 1997/03/26 22:23:15 roberto Exp $
*/

#ifndef lualib_h
#define lualib_h

#include "lua.h"

void iolib_open   (void);
void strlib_open  (void);
void mathlib_open (void);



/* auxiliar functions (private) */

char *luaI_addchar (int c);
void luaI_emptybuff (void);
void luaI_addquoted (char *s);

char *luaL_item_end (char *p);
int luaL_singlematch (int c, char *p);

#endif

