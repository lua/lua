/*
** $Id: lua.c,v 1.55 2000/10/20 16:36:32 roberto Exp $
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


static lua_State *L = NULL;


#ifndef PROMPT
#define PROMPT		"> "
#endif

#ifdef _POSIX_SOURCE
#include <unistd.h>
#else
static int isatty (int x) { return x==0; }  /* assume stdin is a tty */
#endif


/*
** global options
*/
struct Options {
  int toclose;
  int stacksize;
};


typedef void (*handler)(int);  /* type for signal actions */

static void laction (int i);


static lua_Hook old_linehook = NULL;
static lua_Hook old_callhook = NULL;


static void userinit (void) {
  lua_baselibopen(L);
  lua_iolibopen(L);
  lua_strlibopen(L);
  lua_mathlibopen(L);
  lua_dblibopen(L);
  /* add your libraries here */
}


static handler lreset (void) {
  return signal(SIGINT, laction);
}


static void lstop (void) {
  lua_setlinehook(L, old_linehook);
  lua_setcallhook(L, old_callhook);
  lreset();
  lua_error(L, "interrupted!");
}


static void laction (int i) {
  (void)i;  /* to avoid warnings */
  signal(SIGINT, SIG_DFL); /* if another SIGINT happens before lstop,
                              terminate process (default action) */
  old_linehook = lua_setlinehook(L, (lua_Hook)lstop);
  old_callhook = lua_setcallhook(L, (lua_Hook)lstop);
}


static int ldo (int (*f)(lua_State *l, const char *), const char *name) {
  int res;
  handler h = lreset();
  int top = lua_gettop(L);
  res = f(L, name);  /* dostring | dofile */
  lua_settop(L, top);  /* remove eventual results */
  signal(SIGINT, h);  /* restore old action */
  /* Lua gives no message in such cases, so lua.c provides one */
  if (res == LUA_ERRMEM) {
    fprintf(stderr, "lua: memory allocation error\n");
  }
  else if (res == LUA_ERRERR)
    fprintf(stderr, "lua: error in error message\n");
  return res;
}


static void print_message (void) {
  fprintf(stderr,
  "usage: lua [options].  Available options are:\n"
  "  -        execute stdin as a file\n"
  "  -c       close Lua when exiting\n"
  "  -e stat  execute string `stat'\n"
  "  -f name  execute file `name' with remaining arguments in table `arg'\n"
  "  -i       enter interactive mode with prompt\n"
  "  -q       enter interactive mode without prompt\n"
  "  -sNUM    set stack size to NUM (must be the first option)\n"
  "  -v       print version information\n"
  "  a=b      set global `a' to string `b'\n"
  "  name     execute file `name'\n"
);
}


static void print_version (void) {
  printf("%.80s  %.80s\n", LUA_VERSION, LUA_COPYRIGHT);
}


static void assign (char *arg) {
  char *eq = strchr(arg, '=');
  *eq = '\0';  /* spilt `arg' in two strings (name & value) */
  lua_pushstring(L, eq+1);
  lua_setglobal(L, arg);
}


static void getargs (char *argv[]) {
  int i;
  lua_newtable(L);
  for (i=0; argv[i]; i++) {
    /* arg[i] = argv[i] */
    lua_pushnumber(L, i);
    lua_pushstring(L, argv[i]);
    lua_settable(L, -3);
  }
  /* arg.n = maximum index in table `arg' */
  lua_pushstring(L, "n");
  lua_pushnumber(L, i-1);
  lua_settable(L, -3);
}


static int l_getargs (lua_State *l) {
  char **argv = (char **)lua_touserdata(l, -1);
  getargs(argv);
  return 1;
}


static int file_input (const char *argv) {
  int result = ldo(lua_dofile, argv);
  if (result) {
    if (result == LUA_ERRFILE) {
      fprintf(stderr, "lua: cannot execute file ");
      perror(argv);
    }
    return EXIT_FAILURE;
  }
  else
    return EXIT_SUCCESS;
}


/* maximum length of an input string */
#ifndef MAXINPUT
#define MAXINPUT	BUFSIZ
#endif

static void manual_input (int version, int prompt) {
  int cont = 1;
  if (version) print_version();
  while (cont) {
    char buffer[MAXINPUT];
    int i = 0;
    if (prompt) {
      const char *s;
      lua_getglobal(L, "_PROMPT");
      s = lua_tostring(L, -1);
      if (!s) s = PROMPT;
      fputs(s, stdout);
      lua_pop(L, 1);  /* remove global */
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
      else if (i >= MAXINPUT-1) {
        fprintf(stderr, "lua: input line too long\n");
        break;
      }
      else buffer[i++] = (char)c;
    }
    buffer[i] = '\0';
    ldo(lua_dostring, buffer);
    lua_settop(L, 0);  /* remove eventual results */
  }
  printf("\n");
}


static int handle_argv (char *argv[], struct Options *opt) {
  if (opt->stacksize > 0) argv++;  /* skip option `-s' (if present) */
  if (*argv == NULL) {  /* no more arguments? */
    if (isatty(0)) {
      manual_input(1, 1);
    }
    else
      ldo(lua_dofile, NULL);  /* executes stdin as a file */
  }
  else {  /* other arguments; loop over them */
    int i;
    for (i = 0; argv[i] != NULL; i++) {
      if (argv[i][0] != '-') {  /* not an option? */
        if (strchr(argv[i], '='))
          assign(argv[i]);
        else
          if (file_input(argv[i]) != EXIT_SUCCESS)
            return EXIT_FAILURE;  /* stop if file fails */
        }
        else switch (argv[i][1]) {  /* option */
          case 0: {
            ldo(lua_dofile, NULL);  /* executes stdin as a file */
            break;
          }
          case 'i': {
            manual_input(0, 1);
            break;
          }
          case 'q': {
            manual_input(0, 0);
            break;
          }
          case 'c': {
            opt->toclose = 1;
            break;
          }
          case 'v': {
            print_version();
            break;
          }
          case 'e': {
            i++;
            if (argv[i] == NULL) {
              print_message();
              return EXIT_FAILURE;
            }
            if (ldo(lua_dostring, argv[i]) != 0) {
              fprintf(stderr, "lua: error running argument `%.99s'\n", argv[i]);
              return EXIT_FAILURE;
            }
            break;
          }
          case 'f': {
            i++;
            if (argv[i] == NULL) {
              print_message();
              return EXIT_FAILURE;
            }
            getargs(argv+i);  /* collect remaining arguments */
            lua_setglobal(L, "arg");
            return file_input(argv[i]);  /* stop scanning arguments */
          }
          case 's': {
            fprintf(stderr, "lua: stack size (`-s') must be the first option\n");
            return EXIT_FAILURE;
          }
          default: {
            print_message();
            return EXIT_FAILURE;
          }
        }
    }
  }
  return EXIT_SUCCESS;
}


static void getstacksize (int argc, char *argv[], struct Options *opt) {
  if (argc >= 2 && argv[1][0] == '-' && argv[1][1] == 's') {
    int stacksize = atoi(&argv[1][2]);
    if (stacksize <= 0) {
      fprintf(stderr, "lua: invalid stack size ('%.20s')\n", &argv[1][2]);
      exit(EXIT_FAILURE);
    }
    opt->stacksize = stacksize;
  }
  else
    opt->stacksize = 0;  /* no stack size */
}


static void register_getargs (char *argv[]) {
  lua_pushuserdata(L, argv);
  lua_pushcclosure(L, l_getargs, 1);
  lua_setglobal(L, "getargs");
}


int main (int argc, char *argv[]) {
  struct Options opt;
  int status;
  opt.toclose = 0;
  getstacksize(argc, argv, &opt);  /* handle option `-s' */
  L = lua_open(opt.stacksize);  /* create state */
  userinit();  /* open libraries */
  register_getargs(argv);  /* create `getargs' function */
  status = handle_argv(argv+1, &opt);
  if (opt.toclose)
    lua_close(L);
  return status;
}

