/*
** lua.c
** Linguagem para Usuarios de Aplicacao
*/

char *rcs_lua="$Id: lua.c,v 1.1 1993/12/17 18:41:19 celes Exp $";

#include <stdio.h>

#include "lua.h"
#include "lualib.h"

void main (int argc, char *argv[])
{
 int i;
 iolib_open ();
 strlib_open ();
 mathlib_open ();
 if (argc < 2)
 {
   char buffer[2048];
   while (gets(buffer) != 0)
     lua_dostring(buffer);
 }
 else
   for (i=1; i<argc; i++)
    lua_dofile (argv[i]);
}
