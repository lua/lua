/*
** lua.c
** Linguagem para Usuarios de Aplicacao
*/

char *rcs_lua="$Id: lua.c,v 1.1 1993/12/17 18:41:19 celes Stab roberto $";

#include <stdio.h>

#include "lua.h"
#include "lualib.h"


int main (int argc, char *argv[])
{
 int i;
 int result = 0;
 iolib_open ();
 strlib_open ();
 mathlib_open ();
 if (argc < 2)
 {
   char buffer[250];
   while (gets(buffer) != 0)
     result = lua_dostring(buffer);
 }
 else
   for (i=1; i<argc; i++)
    result = lua_dofile (argv[i]);
 return result;
}


