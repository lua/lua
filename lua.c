/*
** $Id: lua.c,v 1.72 2001/11/27 20:56:47 roberto Exp $
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
static int isatty (int x) { return x==0; }  /* assume stdin is a tty */
#endif


#ifndef LUA_PROGNAME
#define LUA_PROGNAME	"lua: "
#endif


#ifndef PROMPT
#define PROMPT		"> "
#endif


#ifndef LUA_USERINIT
#define LUA_USERINIT(L)		openstdlibs(L)
#endif



static lua_State *L = NULL;


typedef void (*handler)(int);  /* type for signal actions */

static void laction (int i);


static lua_Hook old_linehook = NULL;
static lua_Hook old_callhook = NULL;



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


static int ldo (int (*f)(lua_State *l, const char *), const char *name,
                int clear) {
  int res;
  handler h = lreset();
  int top = lua_gettop(L);
  res = f(L, name);  /* dostring | dofile */
  signal(SIGINT, h);  /* restore old action */
  if (clear)
    lua_settop(L, top);  /* remove eventual results */
  /* Lua gives no message in such cases, so lua.c provides one */
  if (res == LUA_ERRMEM) {
    fprintf(stderr, LUA_PROGNAME "memory allocation error\n");
  }
  else if (res == LUA_ERRERR)
    fprintf(stderr, LUA_PROGNAME "error in error message\n");
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
  lua_pushliteral(L, "n");
  lua_pushnumber(L, i-1);
  lua_settable(L, -3);
}


static int l_getargs (lua_State *l) {
  char **argv = (char **)lua_touserdata(l, lua_upvalueindex(1));
  getargs(argv);
  return 1;
}


static int file_input (const char *argv) {
  int result = ldo(lua_dofile, argv, 1);
  if (result) {
    if (result == LUA_ERRFILE) {
      fprintf(stderr, LUA_PROGNAME "cannot execute file ");
      perror(argv);
    }
    return EXIT_FAILURE;
  }
  else
    return EXIT_SUCCESS;
}


/* maximum length of an input line */
#ifndef MAXINPUT
#define MAXINPUT	512
#endif


static const char *get_prompt (int prompt) {
  if (!prompt)
    return "";
  else {
    const char *s;
    lua_getglobal(L, "_PROMPT");
    s = lua_tostring(L, -1);
    if (!s) s = PROMPT;
    lua_pop(L, 1);  /* remove global */
    return s;
  }
}


static void manual_input (int version, int prompt) {
  if (version) print_version();
  for (;;) {
    int firstline = 1;
    int toprint = 0;
    fputs(get_prompt(prompt), stdout);  /* show prompt */
    for(;;) {
      char buffer[MAXINPUT];
      size_t l;
      if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        printf("\n");
        return;
      }
      if (firstline && buffer[0] == '=') {
        buffer[0] = ' ';
        lua_pushstring(L, "return");
        toprint = 1;
      }
      l = strlen(buffer);
      if (buffer[l-1] == '\n' && buffer[l-2] == '\\') {
        buffer[l-2] = '\n';
        lua_pushlstring(L, buffer, l-1);
      }
      else {
        lua_pushlstring(L, buffer, l);
        break;
      }
      firstline = 0;
    }
    lua_concat(L, lua_gettop(L));
    ldo(lua_dostring, lua_tostring(L, 1), 0);
    lua_remove(L, 1);  /* remove ran string */
    if (toprint && lua_gettop(L) > 0) {  /* any result to print? */
      lua_getglobal(L, "print");
      lua_insert(L, 1);
      lua_call(L, lua_gettop(L)-1, 0);
    }
    else
      lua_settop(L, 0);  /* remove eventual results */
  }
}


static int handle_argv (char *argv[], int *toclose) {
  if (*argv == NULL) {  /* no more arguments? */
    if (isatty(0)) {
      manual_input(1, 1);
    }
    else
      ldo(lua_dofile, NULL, 1);  /* executes stdin as a file */
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
            ldo(lua_dofile, NULL, 1);  /* executes stdin as a file */
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
            *toclose = 1;
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
            if (ldo(lua_dostring, argv[i], 1) != 0) {
              fprintf(stderr, LUA_PROGNAME "error running argument `%.99s'\n",
                      argv[i]);
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
            if (i == 0) break;  /* option already handled */
            fprintf(stderr,
               LUA_PROGNAME "stack size (`-s') must be the first option\n");
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


static int getstacksize (int argc, char *argv[]) {
  int stacksize = 0;
  if (argc >= 2 && argv[1][0] == '-' && argv[1][1] == 's') {
    stacksize = strtol(&argv[1][2], NULL, 10);
    if (stacksize <= 0) {
      fprintf(stderr, LUA_PROGNAME "invalid stack size ('%.20s')\n",
              &argv[1][2]);
      exit(EXIT_FAILURE);
    }
  }
  return stacksize;
}


static void register_getargs (char *argv[]) {
  lua_newuserdatabox(L, argv);
  lua_pushcclosure(L, l_getargs, 1);
  lua_setglobal(L, "getargs");
}


static void openstdlibs (lua_State *l) {
  lua_baselibopen(l);
  lua_iolibopen(l);
  lua_strlibopen(l);
  lua_mathlibopen(l);
  lua_dblibopen(l);
}


int main (int argc, char *argv[]) {
  int status;
  int toclose = 0;
  L = lua_open(getstacksize(argc, argv));  /* create state */
  LUA_USERINIT(L);  /* open libraries */
  register_getargs(argv);  /* create `getargs' function */
  status = handle_argv(argv+1, &toclose);
  if (toclose)
    lua_close(L);
  return status;
}

