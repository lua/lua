/*
** $Id: lua.c,v 1.64 2001/02/23 20:28:26 roberto Exp roberto $
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



#ifndef PROMPT
#define PROMPT		l_s("> ")
#endif


#ifndef LUA_USERINIT
#define LUA_USERINIT(L)		openstdlibs(L)
#endif


#ifndef LUA_USERFINI
#define LUA_USERFINI
#endif


/*
** global options
*/
struct Options {
  int toclose;
  int stacksize;
};


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
  lua_error(L, l_s("interrupted!"));
}


static void laction (int i) {
  (void)i;  /* to avoid warnings */
  signal(SIGINT, SIG_DFL); /* if another SIGINT happens before lstop,
                              terminate process (default action) */
  old_linehook = lua_setlinehook(L, (lua_Hook)lstop);
  old_callhook = lua_setcallhook(L, (lua_Hook)lstop);
}


static int ldo (int (*f)(lua_State *l, const l_char *), const l_char *name) {
  int res;
  handler h = lreset();
  int top = lua_gettop(L);
  res = f(L, name);  /* dostring | dofile */
  lua_settop(L, top);  /* remove eventual results */
  signal(SIGINT, h);  /* restore old action */
  /* Lua gives no message in such cases, so lua.c provides one */
  if (res == LUA_ERRMEM) {
    fprintf(stderr, l_s("lua: memory allocation error\n"));
  }
  else if (res == LUA_ERRERR)
    fprintf(stderr, l_s("lua: error in error message\n"));
  return res;
}


static void print_message (void) {
  fprintf(stderr,
  l_s("usage: lua [options].  Available options are:\n")
  l_s("  -        execute stdin as a file\n")
  l_s("  -c       close Lua when exiting\n")
  l_s("  -e stat  execute string `stat'\n")
  l_s("  -f name  execute file `name' with remaining arguments in table `arg'\n")
  l_s("  -i       enter interactive mode with prompt\n")
  l_s("  -q       enter interactive mode without prompt\n")
  l_s("  -sNUM    set stack size to NUM (must be the first option)\n")
  l_s("  -v       print version information\n")
  l_s("  a=b      set global `a' to string `b'\n")
  l_s("  name     execute file `name'\n")
);
}


static void print_version (void) {
  printf(l_s("%.80s  %.80s\n"), LUA_VERSION, LUA_COPYRIGHT);
}


static void assign (l_char *arg) {
  l_char *eq = strchr(arg, l_c('='));
  *eq = l_c('\0');  /* spilt `arg' in two strings (name & value) */
  lua_pushstring(L, eq+1);
  lua_setglobal(L, arg);
}


static void getargs (l_char *argv[]) {
  int i;
  lua_newtable(L);
  for (i=0; argv[i]; i++) {
    /* arg[i] = argv[i] */
    lua_pushnumber(L, i);
    lua_pushstring(L, argv[i]);
    lua_settable(L, -3);
  }
  /* arg.n = maximum index in table `arg' */
  lua_pushliteral(L, l_s("n"));
  lua_pushnumber(L, i-1);
  lua_settable(L, -3);
}


static int l_getargs (lua_State *l) {
  l_char **argv = (l_char **)lua_touserdata(l, -1);
  getargs(argv);
  return 1;
}


static int file_input (const l_char *argv) {
  int result = ldo(lua_dofile, argv);
  if (result) {
    if (result == LUA_ERRFILE) {
      fprintf(stderr, l_s("lua: cannot execute file "));
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


static const l_char *get_prompt (int prompt) {
  if (!prompt)
    return l_s("");
  else {
    const l_char *s;
    lua_getglobal(L, l_s("_PROMPT"));
    s = lua_tostring(L, -1);
    if (!s) s = PROMPT;
    lua_pop(L, 1);  /* remove global */
    return s;
  }
}


static void manual_input (int version, int prompt) {
  if (version) print_version();
  for (;;) {
    fputs(get_prompt(prompt), stdout);  /* show prompt */
    for(;;) {
      l_char buffer[MAXINPUT];
      size_t l;
      if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        printf(l_s("\n"));
        return;
      }
      l = strlen(buffer);
      if (buffer[l-1] == l_c('\n') && buffer[l-2] == l_c('\\')) {
        buffer[l-2] = l_c('\n');
        lua_pushlstring(L, buffer, l-1);
      }
      else {
        lua_pushlstring(L, buffer, l);
        break;
      }
    }
    lua_concat(L, lua_gettop(L));
    ldo(lua_dostring, lua_tostring(L, -1));
    lua_settop(L, 0);  /* remove eventual results */
  }
}


static int handle_argv (l_char *argv[], struct Options *opt) {
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
      if (argv[i][0] != l_c('-')) {  /* not an option? */
        if (strchr(argv[i], l_c('=')))
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
          case l_c('i'): {
            manual_input(0, 1);
            break;
          }
          case l_c('q'): {
            manual_input(0, 0);
            break;
          }
          case l_c('c'): {
            opt->toclose = 1;
            break;
          }
          case l_c('v'): {
            print_version();
            break;
          }
          case l_c('e'): {
            i++;
            if (argv[i] == NULL) {
              print_message();
              return EXIT_FAILURE;
            }
            if (ldo(lua_dostring, argv[i]) != 0) {
              fprintf(stderr, l_s("lua: error running argument `%.99s'\n"), argv[i]);
              return EXIT_FAILURE;
            }
            break;
          }
          case l_c('f'): {
            i++;
            if (argv[i] == NULL) {
              print_message();
              return EXIT_FAILURE;
            }
            getargs(argv+i);  /* collect remaining arguments */
            lua_setglobal(L, l_s("arg"));
            return file_input(argv[i]);  /* stop scanning arguments */
          }
          case l_c('s'): {
            fprintf(stderr, l_s("lua: stack size (`-s') must be the first option\n"));
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


static void getstacksize (int argc, l_char *argv[], struct Options *opt) {
  if (argc >= 2 && argv[1][0] == l_c('-') && argv[1][1] == l_c('s')) {
    int stacksize = strtol(&argv[1][2], NULL, 10);
    if (stacksize <= 0) {
      fprintf(stderr, l_s("lua: invalid stack size ('%.20s')\n"), &argv[1][2]);
      exit(EXIT_FAILURE);
    }
    opt->stacksize = stacksize;
  }
  else
    opt->stacksize = 0;  /* no stack size */
}


static void register_getargs (l_char *argv[]) {
  lua_pushuserdata(L, argv);
  lua_pushcclosure(L, l_getargs, 1);
  lua_setglobal(L, l_s("getargs"));
}


static void openstdlibs (lua_State *l) {
  lua_baselibopen(l);
  lua_iolibopen(l);
  lua_strlibopen(l);
  lua_mathlibopen(l);
  lua_dblibopen(l);
}


int main (int argc, l_char *argv[]) {
  struct Options opt;
  int status;
  opt.toclose = 0;
  getstacksize(argc, argv, &opt);  /* handle option `-s' */
  L = lua_open(opt.stacksize);  /* create state */
  LUA_USERINIT(L);  /* open libraries */
  register_getargs(argv);  /* create `getargs' function */
  status = handle_argv(argv+1, &opt);
  if (opt.toclose)
    lua_close(L);
  return status;
}

