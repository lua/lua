/*
** $Id: lua.c,v 1.9 1997/12/11 17:00:21 roberto Exp roberto $
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

/* command line options:
** -v      print version information (banner).
** -d      debug on
** -e      dostring on next arg
** a=b     sets global 'a' with string 'b'
** -q      interactive mode without prompt
** -i      interactive mode with prompt
** -       executes stdin as a file
** name    dofile "name"
*/


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

#define BUF_SIZE	512

static void manual_input (int prompt)
{
  int cont = 1;
  while (cont) {
    char buffer[BUF_SIZE];
    int i = 0;
    lua_beginblock();
    if (prompt)
      printf("%s", lua_getstring(lua_getglobal("_PROMPT")));
    for(;;) {
      int c = getchar();
      if (c == EOF) {
        cont = 0;
        break;
      }
      else if (c == '\n') {
        if (i>0 && buffer[i-1] == '\\')
          buffer[i-1] = '\n';
        else break;
      }
      else if (i >= BUF_SIZE-1) {
        fprintf(stderr, "lua: argument line too long\n");
        break;
      }
      else buffer[i++] = c;
    }
    buffer[i] = 0;
    lua_dostring(buffer);
    lua_endblock();
  }
  printf("\n");
}


int main (int argc, char *argv[])
{
  int i;
  setlocale(LC_ALL, "");
  lua_iolibopen();
  lua_strlibopen();
  lua_mathlibopen();
  lua_pushstring("> "); lua_setglobal("_PROMPT");
  if (argc < 2) {  /* no arguments? */
    if (isatty(0)) {
      printf("%s  %s\n", LUA_VERSION, LUA_COPYRIGHT);
      manual_input(1);
    }
    else
      lua_dofile(NULL);  /* executes stdin as a file */
  }
  else for (i=1; i<argc; i++) {
    if (strcmp(argv[i], "-") == 0)
      lua_dofile(NULL);  /* executes stdin as a file */
    else if (strcmp(argv[i], "-i") == 0)
      manual_input(1);
    else if (strcmp(argv[i], "-q") == 0)
      manual_input(0);
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
#ifdef DEBUG
  lua_close();
#endif
  return 0;
}

