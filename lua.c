/*
** $Id: $
** Lua stand-alone interpreter
** See Copyright Notice in lua.h
*/


#include <stdio.h>
#include <string.h>

#include "lua.h"
#include "luadebug.h"
#include "lualib.h"


#ifndef OLD_ANSI
#include <locale.h>
#else
#define setlocale(a,b)  0
#endif

#ifdef _POSIX_SOURCE
#include <unistd.h>
#else
#define isatty(x)       (x==0)  /* assume stdin is a tty */
#endif


static void manual_input (void)
{
  if (isatty(0)) {
    char buffer[250];
    while (fgets(buffer, sizeof(buffer), stdin) != 0) {
      lua_beginblock();
      lua_dostring(buffer);
      lua_endblock();
    }
  }
  else
    lua_dofile(NULL);  /* executes stdin as a file */
}


int main (int argc, char *argv[])
{
  int i;
  setlocale(LC_ALL, "");
  lua_iolibopen ();
  lua_strlibopen ();
  lua_mathlibopen ();
  if (argc < 2)
    manual_input();
  else for (i=1; i<argc; i++) {
    if (strcmp(argv[i], "-") == 0)
      manual_input();
    else if (strcmp(argv[i], "-d") == 0)
      lua_debug = 1;
    else if (strcmp(argv[i], "-v") == 0)
      printf("%s  %s\n(written by %s)\n\n",
             LUA_VERSION, LUA_COPYRIGHT, LUA_AUTHORS);
    else if ((strcmp(argv[i], "-e") == 0 && i++) || strchr(argv[i], '=')) {
      if (lua_dostring(argv[i]) != 0) {
        fprintf(stderr, "lua: error running argument `%s'\n", argv[i]);
        return 1;
      }
    }
    else {
      int result = lua_dofile (argv[i]);
      if (result) {
        if (result == 2) {
          fprintf(stderr, "lua: cannot execute file ");
          perror(argv[i]);
        }
        return 1;
      }
    }
  }
  return 0;
}

