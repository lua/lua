/*
** lua.c
** Linguagem para Usuarios de Aplicacao
*/

char *rcs_lua="$Id: lua.c,v 1.4 1995/02/07 16:04:15 lhf Exp $";

#include <stdio.h>
#include <string.h>

#include "lua.h"
#include "lualib.h"

static int lua_argc;
static char **lua_argv;

/*
%F Allow Lua code to access argv strings.
%i Receive from Lua the argument number (starting with 1).
%o Return to Lua the argument, or nil if it does not exist.
*/
static void lua_getargv (void)
{
 lua_Object lo = lua_getparam(1);
 if (!lua_isnumber(lo))
  lua_pushnil();
 else
 {
  int n = (int)lua_getnumber(lo);
  if (n < 1 || n > lua_argc) lua_pushnil();
  else                       lua_pushstring(lua_argv[n]);
 }
}


int main (int argc, char *argv[])
{
 int i;
 int result = 0;
 iolib_open ();
 strlib_open ();
 mathlib_open ();

 lua_register("argv", lua_getargv);

 if (argc < 2)
 {
   char buffer[250];
   while (gets(buffer) != 0)
     result = lua_dostring(buffer);
 }
 else
 {
  for (i=1; i<argc; i++)
  {
   if (strcmp(argv[i], "--") == 0)
   {
    lua_argc = argc-i-1;
    lua_argv = argv+i;
    break;
   }
  }
  for (i=1; i<argc; i++)
  {
   if (strcmp(argv[i], "--") == 0)
    break;
   else
    result = lua_dofile (argv[i]);
  }
 }
 return result;
}


