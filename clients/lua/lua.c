/*
** lua.c
** Linguagem para Usuarios de Aplicacao
*/

char *rcs_lua="$Id: lua.c,v 1.10 1996/05/03 19:20:17 roberto Exp $";

#include <stdio.h>
#include <string.h>

#include "lua.h"
#include "lualib.h"

static int lua_argc;
static char **lua_argv;

#ifdef POSIX
/*
** although this function is POSIX, there is no standard header file that
** defines it
*/
int isatty (int fd);
#else
#define isatty(x)       (x==0)  /* assume stdin is a tty */
#endif

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


static void manual_input (void)
{
  if (isatty(0))
  {
   char buffer[250];
   while (fgets(buffer, sizeof(buffer), stdin) != 0)
     lua_dostring(buffer);
  }
  else
    lua_dofile(NULL);  /* executes stdin as a file */
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
   manual_input();
 else
 {
  for (i=1; i<argc; i++)
   if (strcmp(argv[i], "--") == 0)
   {
    lua_argc = argc-i-1;
    lua_argv = argv+i;
    break;
   }
  for (i=1; i<argc; i++)
  {
   if (strcmp(argv[i], "--") == 0)
    break;
   else if (strcmp(argv[i], "-") == 0)
    manual_input();
   else if (strcmp(argv[i], "-v") == 0)
    printf("%s  %s\n(written by %s)\n\n",
            LUA_VERSION, LUA_COPYRIGHT, LUA_AUTHORS);
   else
   {
    result = lua_dofile (argv[i]);
    if (result)
      fprintf(stderr, "lua: error trying to run file %s\n", argv[i]);
   }
  }
 }
 return result;
}


