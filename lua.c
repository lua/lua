/*
** $Id: lua.c,v 1.97 2002/07/10 20:49:01 roberto Exp roberto $
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



#ifndef PROMPT
#define PROMPT		"> "
#endif


#ifndef PROMPT2
#define PROMPT2		">> "
#endif


#ifndef lua_userinit
#define lua_userinit(L)		openstdlibs(L)
#endif


static lua_State *L = NULL;

static const char *progname;


static lua_Hook old_hook = NULL;
static int old_mask = 0;


static void lstop (lua_State *l, lua_Debug *ar) {
  (void)ar;  /* unused arg. */
  lua_sethook(l, old_hook, old_mask);
  luaL_error(l, "interrupted!");
}


static void laction (int i) {
  signal(i, SIG_DFL); /* if another SIGINT happens before lstop,
                              terminate process (default action) */
  old_hook = lua_gethook(L);
  old_mask = lua_gethookmask(L);
  lua_sethook(L, lstop, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT(1));
}


static void print_usage (void) {
  fprintf(stderr,
  "usage: %s [options] [prog [args]].\n"
  "Available options are:\n"
  "  -        execute stdin as a file\n"
  "  -e stat  execute string `stat'\n"
  "  -i       enter interactive mode after executing `prog'\n"
  "  -l name  execute file `name'\n"
  "  -v       print version information\n"
  "  --       stop handling options\n" ,
  progname);
}


static void l_message (const char *pname, const char *msg) {
  size_t l = strlen(msg);
  if (pname) fprintf(stderr, "%s: ", pname);
  fprintf(stderr, "%s", msg);
  if (l > 0 && msg[l-1] != '\n')  /* does not end with newline? */
    fprintf(stderr, "\n");  /* add a newline */
}


static void report (int status) {
  const char *msg;
  if (status) {
    msg = lua_tostring(L, -1);
    if (msg == NULL) msg = "(no message)";
    l_message(progname, msg);
    lua_pop(L, 1);
  }
}


static int lcall (int clear) {
  int status;
  int top = lua_gettop(L);
  lua_getglobal(L, "_TRACEBACK");  /* get traceback function */
  lua_insert(L, top);  /* put it under chunk */
  signal(SIGINT, laction);
  status = lua_pcall(L, 0, LUA_MULTRET, -2);
  signal(SIGINT, SIG_DFL);
  lua_remove(L, top);  /* remove traceback function */
  if (status == 0 && clear)
    lua_settop(L, top);  /* remove eventual results */
  return status;
}


static int l_panic (lua_State *l) {
  (void)l;
  l_message(progname, "unable to recover; exiting");
  return 0;
}


static void print_version (void) {
  l_message(NULL, LUA_VERSION "  " LUA_COPYRIGHT);
}


static void getargs (char *argv[], int n) {
  int i;
  lua_newtable(L);
  for (i=0; argv[i]; i++) {
    lua_pushnumber(L, i - n);
    lua_pushstring(L, argv[i]);
    lua_rawset(L, -3);
  }
  /* arg.n = maximum index in table `arg' */
  lua_pushliteral(L, "n");
  lua_pushnumber(L, i-n-1);
  lua_rawset(L, -3);
}


static int docall (int status) {
  if (status == 0) status = lcall(1);
  report(status);
  return status;
}


static int file_input (const char *name) {
  return docall(luaL_loadfile(L, name));
}


static int dostring (const char *s, const char *name) {
  return docall(luaL_loadbuffer(L, s, strlen(s), name));
}


/*
** this macro can be used by some `history' system to save lines
** read in manual input
*/
#ifndef lua_saveline
#define lua_saveline(L,line)	/* empty */
#endif


/*
** this macro defines a function to show the prompt and reads the
** next line for manual input
*/
#ifndef lua_readline
#define lua_readline(L,prompt)		readline(L,prompt)

/* maximum length of an input line */
#ifndef MAXINPUT
#define MAXINPUT	512
#endif


static int readline (lua_State *l, const char *prompt) {
  static char buffer[MAXINPUT];
  if (prompt) {
    fputs(prompt, stdout);
    fflush(stdout);
  }
  if (fgets(buffer, sizeof(buffer), stdin) == NULL)
    return 0;  /* read fails */
  else {
    lua_pushstring(l, buffer);
    return 1;
  }
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
  int status;
  lua_settop(L, 0);
  if (lua_readline(L, get_prompt(1)) == 0)  /* no input? */
    return -1;
  if (lua_tostring(L, -1)[0] == '=') {  /* line starts with `=' ? */
    lua_pushfstring(L, "return %s", lua_tostring(L, -1)+1);/* `=' -> `return' */
    lua_remove(L, -2);  /* remove original line */
  }
  for (;;) {  /* repeat until gets a complete line */
    status = luaL_loadbuffer(L, lua_tostring(L, 1), lua_strlen(L, 1), "=stdin");
    if (!incomplete(status)) break;  /* cannot try to add lines? */
    if (lua_readline(L, get_prompt(0)) == 0)  /* no more input? */
      return -1;
    lua_concat(L, lua_gettop(L));  /* join lines */
  }
  lua_saveline(L, lua_tostring(L, 1));
  lua_remove(L, 1);  /* remove line */
  return status;
}


static void manual_input (void) {
  int status;
  const char *oldprogname = progname;
  progname = NULL;
  while ((status = load_string()) != -1) {
    if (status == 0) status = lcall(0);
    report(status);
    if (status == 0 && lua_gettop(L) > 0) {  /* any result to print? */
      lua_getglobal(L, "print");
      lua_insert(L, 1);
      if (lua_pcall(L, lua_gettop(L)-1, 0, 0) != 0)
        l_message(progname, "error calling `print'");
    }
  }
  lua_settop(L, 0);  /* clear stack */
  fputs("\n", stdout);
  progname = oldprogname;
}


static int handle_argv (char *argv[], int *interactive) {
  if (argv[1] == NULL) {  /* no more arguments? */
    if (isatty(0)) {
      print_version();
      manual_input();
    }
    else
      file_input(NULL);  /* executes stdin as a file */
  }
  else {  /* other arguments; loop over them */
    int i;
    for (i = 1; argv[i] != NULL; i++) {
      if (argv[i][0] != '-') break;  /* not an option? */
      switch (argv[i][1]) {  /* option */
        case '-': {  /* `--' */
          i++;  /* skip this argument */
          goto endloop;  /* stop handling arguments */
        }
        case '\0': {
          file_input(NULL);  /* executes stdin as a file */
          break;
        }
        case 'i': {
          *interactive = 1;
          break;
        }
        case 'v': {
          print_version();
          break;
        }
        case 'e': {
          const char *chunk = argv[i] + 2;
          if (*chunk == '\0') chunk = argv[++i];
          if (chunk == NULL) {
            print_usage();
            return EXIT_FAILURE;
          }
          if (dostring(chunk, "=<command line>") != 0)
            return EXIT_FAILURE;
          break;
        }
        case 'l': {
          const char *filename = argv[i] + 2;
          if (*filename == '\0') filename = argv[++i];
          if (filename == NULL) {
            print_usage();
            return EXIT_FAILURE;
          }
          if (file_input(filename))
            return EXIT_FAILURE;  /* stop if file fails */
          break;
        }
        case 'c': {
          l_message(progname, "option `-c' is deprecated");
          break;
        }
        case 's': {
          l_message(progname, "option `-s' is deprecated");
          break;
        }
        default: {
          print_usage();
          return EXIT_FAILURE;
        }
      }
    } endloop:
    if (argv[i] != NULL) {
      const char *filename = argv[i];
      getargs(argv, i);  /* collect remaining arguments */
      lua_setglobal(L, "arg");
      return file_input(filename);  /* stop scanning arguments */
    }
  }
  return EXIT_SUCCESS;
}


static int openstdlibs (lua_State *l) {
  return lua_baselibopen(l) +
         lua_tablibopen(l) +
         lua_iolibopen(l) +
         lua_strlibopen(l) +
         lua_mathlibopen(l) +
         lua_dblibopen(l) +
         /* add your libraries here */
         0;
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
  int interactive = 0;
  (void)argc;  /* to avoid warnings */
  progname = argv[0];
  L = lua_open();  /* create state */
  lua_atpanic(L, l_panic);
  lua_pop(L, lua_userinit(L));  /* open libraries, discard any results */
  status = handle_luainit();
  if (status != 0) return status;
  status = handle_argv(argv, &interactive);
  if (status == 0 && interactive) manual_input();
  lua_close(L);
  return status;
}

