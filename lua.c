/*
** $Id: lua.c,v 1.32 2000/01/19 16:50:14 roberto Exp roberto $
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


#ifndef PROMPT
#define PROMPT		"> "
#endif

#ifdef _POSIX_SOURCE
#include <unistd.h>
#else
static int isatty (int x) { return x==0; }  /* assume stdin is a tty */
#endif

typedef void (*handler)(int);  /* type for signal actions */

static void laction (int i);


static lua_Dbghook old_linehook = NULL;
static lua_Dbghook old_callhook = NULL;


static handler lreset (void) {
  return signal(SIGINT, laction);
}


static void lstop (void) {
  lua_setlinehook(lua_state, old_linehook);
  lua_setcallhook(lua_state, old_callhook);
  lreset();
  lua_error("interrupted!");
}


static void laction (int i) {
  (void)i;  /* to avoid warnings */
  signal(SIGINT, SIG_DFL); /* if another SIGINT happens before lstop,
                              terminate process (default action) */
  old_linehook = lua_setlinehook(lua_state, (lua_Dbghook)lstop);
  old_callhook = lua_setcallhook(lua_state, (lua_Dbghook)lstop);
}


static int ldo (int (*f)(lua_State *L, const char *), const char *name) {
  int res;
  handler h = lreset();
  res = f(lua_state, name);  /* dostring | dofile */
  signal(SIGINT, h);  /* restore old action */
  return res;
}


static void print_message (void) {
  fprintf(stderr,
  "usage: lua [options].  Available options are:\n"
  "  -        execute stdin as a file\n"
  "  -d       turn debug on\n"
  "  -e stat  execute string `stat'\n"
  "  -f name  execute file `name' with remaining arguments in table `arg'\n"
  "  -i       enter interactive mode with prompt\n"
  "  -q       enter interactive mode without prompt\n"
  "  -v       print version information\n"
  "  a=b      set global `a' to string `b'\n"
  "  name     execute file `name'\n"
);
}


static void print_version (void) {
  printf("%s  %s\n", LUA_VERSION, LUA_COPYRIGHT);
}


static void assign (char *arg) {
  char buffer[500];
  if (strlen(arg) >= sizeof(buffer))
    fprintf(stderr, "lua: shell argument too long");
  else {
    char *eq = strchr(arg, '=');
    lua_pushstring(eq+1);
    strncpy(buffer, arg, eq-arg);
    buffer[eq-arg] = 0;
    lua_setglobal(buffer);
  }
}


static void getargs (int argc, char *argv[]) {
  lua_beginblock(); {
  int i;
  lua_Object args = lua_createtable();
  lua_pushobject(args);
  lua_setglobal("arg");
  for (i=0; i<argc; i++) {
    /* arg[i] = argv[i] */
    lua_pushobject(args); lua_pushnumber(i);
    lua_pushstring(argv[i]); lua_settable();
  }
  /* arg.n = maximum index in table `arg' */
  lua_pushobject(args); lua_pushstring("n");
  lua_pushnumber(argc-1); lua_settable();
  } lua_endblock();
}


static void file_input (char **argv, int arg) {
  int result = ldo(lua_dofile, argv[arg]);
  if (result) {
    if (result == 2) {
      fprintf(stderr, "lua: cannot execute file ");
      perror(argv[arg]);
    }
    exit(1);
  }
}


static void manual_input (int prompt) {
  int cont = 1;
  while (cont) {
    char buffer[BUFSIZ];
    int i = 0;
    lua_beginblock();
    if (prompt) {
      const char *s = lua_getstring(lua_getglobal("_PROMPT"));
      if (!s) s = PROMPT;
      printf("%s", s);
    }
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


int main (int argc, char *argv[]) {
  int i;
  lua_state = lua_newstate("stack", 1024, "builtin", 1, NULL);
  lua_userinit();
  if (argc < 2) {  /* no arguments? */
    if (isatty(0)) {
      print_version();
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
          lua_setdebug(lua_state, 1);
          if (i==argc-1) {  /* last argument? */
            print_version();
            manual_input(1);
          }
          break;
        case 'v':
          print_version();
          break;
        case 'e':
          i++;
          if (i>=argc) {
            print_message();
            exit(1);
          }
          if (ldo(lua_dostring, argv[i]) != 0) {
            fprintf(stderr, "lua: error running argument `%s'\n", argv[i]);
            exit(1);
          }
          break;
        case 'f':
          i++;
          if (i>=argc) {
            print_message();
            exit(1);
          }
          getargs(argc-i, argv+i);  /* collect following arguments */
          file_input(argv, i);
          i = argc;  /* stop running arguments */
          break;
        default:
          print_message();
          exit(1);
      }
    }
    else if (strchr(argv[i], '='))
      assign(argv[i]);
    else
      file_input(argv, i);
  }
#ifdef DEBUG
  lua_close();
#endif
  return 0;
}

