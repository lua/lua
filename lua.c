/*
** $Id: lua.c,v 1.77 2002/02/14 22:23:43 roberto Exp roberto $
** Lua stand-alone interpreter
** See Copyright Notice in lua.h
*/


#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"

#include "lauxlib.h"
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


#ifndef PROMPT2
#define PROMPT2		">> "
#endif


#ifndef LUA_USERINIT
#define LUA_USERINIT(L)		openstdlibs(L)
#endif


static lua_State *L = NULL;


static lua_Hook old_linehook = NULL;
static lua_Hook old_callhook = NULL;


static void lstop (void) {
  lua_setlinehook(L, old_linehook);
  lua_setcallhook(L, old_callhook);
  lua_error(L, "interrupted!");
}


static void laction (int i) {
  (void)i;  /* to avoid warnings */
  signal(SIGINT, SIG_DFL); /* if another SIGINT happens before lstop,
                              terminate process (default action) */
  old_linehook = lua_setlinehook(L, (lua_Hook)lstop);
  old_callhook = lua_setcallhook(L, (lua_Hook)lstop);
}


/* Lua gives no message in such cases, so we provide one */
static void report (int result) {
  if (result == LUA_ERRMEM || result == LUA_ERRERR)
    fprintf(stderr, "%s%s\n", LUA_PROGNAME, luaL_errstr(result));
}


static int ldo (int (*f)(lua_State *l, const char *), const char *name,
                int clear) {
  int result;
  int top = lua_gettop(L);
  signal(SIGINT, laction);
  result = f(L, name);  /* dostring | dofile */
  signal(SIGINT, SIG_DFL);
  if (clear) lua_settop(L, top);  /* remove eventual results */
  report(result);
  return result;
}


static void print_usage (void) {
  fprintf(stderr,
  "usage: lua [options].  Available options are:\n"
  "  -        execute stdin as a file\n"
  "  -c       close Lua when exiting\n"
  "  -e stat  execute string `stat'\n"
  "  -f name  execute file `name' with remaining arguments in table `arg'\n"
  "  -i       enter interactive mode\n"
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
    lua_rawset(L, -3);
  }
  /* arg.n = maximum index in table `arg' */
  lua_pushliteral(L, "n");
  lua_pushnumber(L, i-1);
  lua_rawset(L, -3);
}


static int l_getargs (lua_State *l) {
  char **argv = (char **)lua_touserdata(l, lua_upvalueindex(1));
  getargs(argv);
  return 1;
}


static int file_input (const char *name) {
  int result = ldo(lua_dofile, name, 1);
  if (result) {
    if (result == LUA_ERRFILE) {
      fprintf(stderr, "%s%s ", LUA_PROGNAME, luaL_errstr(result));
      perror(name);
    }
    return EXIT_FAILURE;
  }
  else
    return EXIT_SUCCESS;
}


#ifdef USE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#define save_line(b)	if (strcspn(b, " \t\n") != 0) add_history(b)
#define push_line(b)	if (incomplete) lua_pushstring(L, "\n"); lua_pushstring(L, b); free(b)
#else
#define save_line(b)
#define push_line(b)	lua_pushstring(L, b)

/* maximum length of an input line */
#ifndef MAXINPUT
#define MAXINPUT	512
#endif


static char *readline (const char *prompt) {
  static char buffer[MAXINPUT];
  if (prompt) {
    fputs(prompt, stdout);
    fflush(stdout);
  }
  return fgets(buffer, sizeof(buffer), stdin);
}
#endif


static const char *get_prompt (int incomplete) {
  const char *p = NULL;
  lua_getglobal(L, incomplete ? "_PROMPT2" : "_PROMPT");
  p = lua_tostring(L, -1);
  if (p == NULL) p = (incomplete ? PROMPT2 : PROMPT);
  lua_pop(L, 1);  /* remove global */
  return p;
}


static int incomplete = 0;

static int trap_eof (lua_State *l) {
 const char *s = lua_tostring(l, 1);
 if (strstr(s, "last token read: `<eof>'") != NULL)
   incomplete = 1;
 else
   fprintf(stderr, "error: %s\n", s);
 return 0;
}


static int load_string (void) {
  lua_getglobal(L, LUA_ERRORMESSAGE);
  lua_pushvalue(L, 1);
  lua_setglobal(L, LUA_ERRORMESSAGE);
  incomplete = 0;
  for (;;) {  /* repeat until gets a complete line */
    int result;
    char *buffer = readline(get_prompt(incomplete));
    if (buffer == NULL) {  /* input end? */
      lua_settop(L, 2);
      lua_setglobal(L, LUA_ERRORMESSAGE);
      return 0;
    }
    if (!incomplete && buffer[0] == '=') {
      buffer[0] = ' ';
      lua_pushstring(L, "return");
    }
    push_line(buffer);
    lua_concat(L, lua_gettop(L)-2);
    incomplete = 0;
    result = lua_loadbuffer(L, lua_tostring(L, 3), lua_strlen(L, 3), "=stdin");
    if (incomplete) continue;  /* repeat loop to get rest of `line' */
    save_line(lua_tostring(L, 3));
    lua_remove(L, 3);
    if (result == 0) {
      lua_insert(L, 2);  /* swap compiled chunk with old _ERRORMESSAGE */
      lua_setglobal(L, LUA_ERRORMESSAGE);  /* restore old _ERRORMESSAGE */
      return 1;
    }
    else
      report(result);
 }
}


static int lcall (lua_State *l, const char *name) {
  (void)name;  /* to avoid warnings */
  return lua_call(l, 0, LUA_MULTRET);
}


static void manual_input (int version) {
  if (version) print_version();
  lua_pushcfunction(L, trap_eof);  /* set up handler for incomplete lines */
  while (load_string()) {
    ldo(lcall, NULL, 0);
    if (lua_gettop(L) > 1) {  /* any result to print? */
      lua_getglobal(L, "print");
      lua_insert(L, 2);
      lua_call(L, lua_gettop(L)-2, 0);
    }
    else
      lua_settop(L, 1);  /* remove eventual results */
  }
  printf("\n");
  lua_settop(L, 0);  /* remove trap_eof */
}


static int handle_argv (char *argv[], int *toclose) {
  if (*argv == NULL) {  /* no more arguments? */
    if (isatty(0)) {
      manual_input(1);
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
          case '\0': {
            ldo(lua_dofile, NULL, 1);  /* executes stdin as a file */
            break;
          }
          case 'i': {
            manual_input(0);
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
              print_usage();
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
              print_usage();
              return EXIT_FAILURE;
            }
            getargs(argv+i);  /* collect remaining arguments */
            lua_setglobal(L, "arg");
            return file_input(argv[i]);  /* stop scanning arguments */
          }
          case 's': {
            fprintf(stderr, LUA_PROGNAME
                    "option `-s' is deprecated (dynamic stack now)\n");
            break;
          }
          default: {
            print_usage();
            return EXIT_FAILURE;
          }
        }
    }
  }
  return EXIT_SUCCESS;
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
  (void)argc;  /* to avoid warnings */
  L = lua_open();  /* create state */
  LUA_USERINIT(L);  /* open libraries */
  register_getargs(argv);  /* create `getargs' function */
  status = handle_argv(argv+1, &toclose);
  if (toclose)
    lua_close(L);
  return status;
}

