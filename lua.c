/*
** $Id: lua.c,v 1.6 1997/12/01 20:31:25 roberto Exp roberto $
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


static void assign (char *arg)
{
  if (strlen(arg) >= 500)
    fprintf(stderr, "lua: shell argument too long");
  else {
    char buffer[500];
    char *eq = strchr(arg, '=');
    lua_pushstring(eq+1);
    strncpy(buffer, arg, eq-arg);
    buffer[eq-arg] = 0;
    lua_setglobal(buffer);
  }
}


static void manual_input (void)
{
  if (isatty(0)) {
    char buffer[250];
    while (1) {
      lua_beginblock();
      printf("%s", lua_getstring(lua_getglobal("_PROMPT")));
      if (fgets(buffer, sizeof(buffer), stdin) == 0)
        break;
      lua_dostring(buffer);
      lua_endblock();
    }
    printf("\n");
  }
  else
    lua_dofile(NULL);  /* executes stdin as a file */
}


int main (int argc, char *argv[])
{
  int i;
  setlocale(LC_ALL, "");
  lua_iolibopen();
  lua_strlibopen();
  lua_mathlibopen();
  lua_pushstring("> "); lua_setglobal("_PROMPT");
  if (argc < 2) {
    printf("%s  %s\n", LUA_VERSION, LUA_COPYRIGHT);
    manual_input();
  }
  else for (i=1; i<argc; i++) {
    if (strcmp(argv[i], "-") == 0)
      manual_input();
    else if (strcmp(argv[i], "-d") == 0)
      lua_debug = 1;
    else if (strcmp(argv[i], "-v") == 0)
      printf("%s  %s\n(written by %s)\n\n",
             LUA_VERSION, LUA_COPYRIGHT, LUA_AUTHORS);
    else if (strcmp(argv[i], "-e") == 0) {
      i++;
      if (lua_dostring(argv[i]) != 0) {
        fprintf(stderr, "lua: error running argument `%s'\n", argv[i]);
        return 1;
      }
    }
    else if (strchr(argv[i], '='))
      assign(argv[i]);
    else {
      int result = lua_dofile(argv[i]);
      if (result) {
        if (result == 2) {
          fprintf(stderr, "lua: cannot execute file ");
          perror(argv[i]);
        }
        return 1;
      }
    }
  }
#if DEBUG
  lua_close();
#endif
  return 0;
}

