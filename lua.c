/*
** $Id: lua.c,v 1.87 2002/05/16 19:09:19 roberto Exp roberto $
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
#define LUA_PROGNAME	"lua"
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
  signal(i, SIG_DFL); /* if another SIGINT happens before lstop,
                              terminate process (default action) */
  old_linehook = lua_setlinehook(L, (lua_Hook)lstop);
  old_callhook = lua_setcallhook(L, (lua_Hook)lstop);
}


static void report (int status) {
  if (status) {
    lua_getglobal(L, "_ALERT");
    lua_pushvalue(L, -2);
    lua_pcall(L, 1, 0, 0);
    lua_pop(L, 1);
  }
}


static int lcall (int clear) {
  int status;
  int top = lua_gettop(L);
  lua_getglobal(L, "_ERRORMESSAGE");
  lua_insert(L, top);
  signal(SIGINT, laction);
  status = lua_pcall(L, 0, LUA_MULTRET, top);
  signal(SIGINT, SIG_DFL);
  if (status == 0) {
    if (clear) lua_settop(L, top);  /* remove eventual results */
    else lua_remove(L, top);  /* else remove only error function */
  }
  return status;
}


static void print_usage (void) {
  fprintf(stderr,
  "usage: %s [options].  Available options are:\n"
  "  -        execute stdin as a file\n"
  "  -c       close Lua when exiting\n"
  "  -e stat  execute string `stat'\n"
  "  -f name  execute file `name' with remaining arguments in table `arg'\n"
  "  -i       enter interactive mode\n"
  "  -v       print version information\n"
  "  a=b      set global `a' to string `b'\n"
  "  name     execute file `name'\n",
  LUA_PROGNAME);
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


static int l_alert (lua_State *l) {
  fputs(luaL_check_string(l, 1), stderr);
  putc('\n', stderr);
  return 0;
}


static int l_getargs (lua_State *l) {
  char **argv = (char **)lua_touserdata(l, lua_upvalueindex(1));
  getargs(argv);
  return 1;
}


static int docall (int status) {
  if (status == 0) status = lcall(1);
  report(status);
  return status;
}


static int file_input (const char *name) {
  return docall(lua_loadfile(L, name));
}


static int dostring (const char *s, const char *name) {
  return docall(lua_loadbuffer(L, s, strlen(s), name));
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


static const char *get_prompt (int firstline) {
  const char *p = NULL;
  lua_getglobal(L, firstline ? "_PROMPT" : "_PROMPT2");
  p = lua_tostring(L, -1);
  if (p == NULL) p = (firstline ? PROMPT : PROMPT2);
  lua_pop(L, 1);  /* remove global */
  return p;
}


static int incomplete (int status) {
  if (status == LUA_ERRSYNTAX &&
         strstr(lua_tostring(L, -1), "near `<eof>'") != NULL) {
    lua_pop(L, 1);
    return 1;
  }
  else
    return 0;
}


static int load_string (void) {
  int firstline = 1;
  int status;
  lua_settop(L, 0);
  do {  /* repeat until gets a complete line */
    char *buffer = readline(get_prompt(firstline));
    if (buffer == NULL) {  /* input end? */
      lua_settop(L, 0);
      return -1;  /* input end */
    }
    if (firstline && buffer[0] == '=') {
      buffer[0] = ' ';
      lua_pushstring(L, "return");
    }
    firstline = 0;
    push_line(buffer);
    lua_concat(L, lua_gettop(L));
    status = lua_loadbuffer(L, lua_tostring(L, 1), lua_strlen(L, 1), "=stdin");
  } while (incomplete(status));  /* repeat loop to get rest of `line' */
  save_line(lua_tostring(L, 1));
  lua_remove(L, 1);
  return status;
}


static void manual_input (int version) {
  int status;
  if (version) print_version();
  while ((status = load_string()) != -1) {
    if (status == 0) status = lcall(0);
    report(status);
    if (status == 0 && lua_gettop(L) > 0) {  /* any result to print? */
      lua_getglobal(L, "print");
      lua_insert(L, 1);
      lua_call(L, lua_gettop(L)-1, 0);
    }
  }
  printf("\n");
}


static int handle_argv (char *argv[], int *toclose) {
  if (*argv == NULL) {  /* no more arguments? */
    if (isatty(0)) {
      manual_input(1);
    }
    else
      file_input(NULL);  /* executes stdin as a file */
  }
  else {  /* other arguments; loop over them */
    int i;
    for (i = 0; argv[i] != NULL; i++) {
      if (argv[i][0] != '-') {  /* not an option? */
        if (strchr(argv[i], '='))
          assign(argv[i]);
        else
          if (file_input(argv[i]))
            return EXIT_FAILURE;  /* stop if file fails */
        }
        else switch (argv[i][1]) {  /* option */
          case '\0': {
            file_input(NULL);  /* executes stdin as a file */
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
            if (dostring(argv[i], "=prog. argument") != 0) {
              fprintf(stderr, "%s: error running argument `%.99s'\n",
                      LUA_PROGNAME, argv[i]);
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
            fprintf(stderr,
                    "%s: option `-s' is deprecated (dynamic stack now)\n",
                    LUA_PROGNAME);
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


static void register_own (char *argv[]) {
  lua_pushudataval(L, argv);
  lua_pushcclosure(L, l_getargs, 1);
  lua_setglobal(L, "getargs");
  lua_register(L, "_ALERT", l_alert);
}


static void openstdlibs (lua_State *l) {
  lua_baselibopen(l);
  lua_tablibopen(l);
  lua_iolibopen(l);
  lua_strlibopen(l);
  lua_mathlibopen(l);
  lua_dblibopen(l);
}


static int handle_luainit (void) {
  const char *init = getenv("LUA_INIT");
  if (init == NULL) return 0;  /* status OK */
  else if (init[0] == '@')
    return file_input(init+1);
  else
    return dostring(init, "=LUA_INIT");
}


int main (int argc, char *argv[]) {
  int status;
  int toclose = 0;
  (void)argc;  /* to avoid warnings */
  L = lua_open();  /* create state */
  LUA_USERINIT(L);  /* open libraries */
  register_own(argv);  /* create own function */
  status = handle_luainit();
  if (status != 0) return status;
  status = handle_argv(argv+1, &toclose);
  if (toclose)
    lua_close(L);
  return status;
}

