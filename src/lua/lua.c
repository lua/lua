/*
** $Id: lua.c,v 1.21 1999/07/02 18:22:38 roberto Exp $
** Lua stand-alone interpreter
** See Copyright Notice in lua.h
*/


#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"
#include "luadebug.h"
#include "lualib.h"


#ifdef _POSIX_SOURCE
#include <unistd.h>
#else
#define isatty(x)       (x==0)  /* assume stdin is a tty */
#endif


typedef void (*handler)(int);  /* type for signal actions */

static void laction (int i);


static lua_LHFunction old_linehook = NULL;
static lua_CHFunction old_callhook = NULL;


static handler lreset (void) {
  return signal(SIGINT, laction);
}


static void lstop (void) {
  lua_setlinehook(old_linehook);
  lua_setcallhook(old_callhook);
  lreset();
  lua_error("interrupted!");
}


static void laction (int i) {
  signal(SIGINT, SIG_DFL); /* if another SIGINT happens before lstop,
                              terminate process (default action) */
  old_linehook = lua_setlinehook((lua_LHFunction)lstop);
  old_callhook = lua_setcallhook((lua_CHFunction)lstop);
}


static int ldo (int (*f)(char *), char *name) {
  int res;
  handler h = lreset();
  res = f(name);  /* dostring | dofile */
  signal(SIGINT, h);  /* restore old action */
  return res;
}


static void print_message (void) {
  fprintf(stderr,
"Lua: command line options:\n"
"  -v       print version information\n"
"  -d       turn debug on\n"
"  -e stat  dostring `stat'\n"
"  -q       interactive mode without prompt\n"
"  -i       interactive mode with prompt\n"
"  -        executes stdin as a file\n"
"  a=b      sets global `a' with string `b'\n"
"  name     dofile `name'\n\n");
}


static void assign (char *arg) {
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


static void manual_input (int prompt) {
  int cont = 1;
  while (cont) {
    char buffer[BUFSIZ];
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
      else if (i >= BUFSIZ-1) {
        fprintf(stderr, "lua: argument line too long\n");
        break;
      }
      else buffer[i++] = (char)c;
    }
    buffer[i] = '\0';
    ldo(lua_dostring, buffer);
    lua_endblock();
  }
  printf("\n");
}


int main (int argc, char *argv[])
{
  int i;
  lua_open();
  lua_pushstring("> "); lua_setglobal("_PROMPT");
  lua_userinit();
  if (argc < 2) {  /* no arguments? */
    if (isatty(0)) {
      printf("%s  %s\n", LUA_VERSION, LUA_COPYRIGHT);
      manual_input(1);
    }
    else
      ldo(lua_dofile, NULL);  /* executes stdin as a file */
  }
  else for (i=1; i<argc; i++) {
    if (argv[i][0] == '-') {  /* option? */
      switch (argv[i][1]) {
        case 0:
          ldo(lua_dofile, NULL);  /* executes stdin as a file */
          break;
        case 'i':
          manual_input(1);
          break;
        case 'q':
          manual_input(0);
          break;
        case 'd':
          lua_setdebug(1);
          break;
        case 'v':
          printf("%s  %s\n(written by %s)\n\n",
                 LUA_VERSION, LUA_COPYRIGHT, LUA_AUTHORS);
          break;
        case 'e':
          i++;
          if (ldo(lua_dostring, argv[i]) != 0) {
            fprintf(stderr, "lua: error running argument `%s'\n", argv[i]);
            return 1;
          }
          break;
        default:
          print_message();
          exit(1);
      }
    }
    else if (strchr(argv[i], '='))
      assign(argv[i]);
    else {
      int result = ldo(lua_dofile, argv[i]);
      if (result) {
        if (result == 2) {
          fprintf(stderr, "lua: cannot execute file ");
          perror(argv[i]);
        }
        exit(1);
      }
    }
  }
#ifdef DEBUG
  lua_close();
#endif
  return 0;
}

