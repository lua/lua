/*
** lua.c
** Linguagem para Usuarios de Aplicacao
*/

char *rcs_lua="$Id: lua.c,v 1.18 1997/06/19 18:55:40 roberto Exp $";

#include <stdio.h>
#include <string.h>

#include "lua.h"
#include "auxlib.h"
#include "lualib.h"


#ifdef _POSIX_SOURCE
#include <unistd.h>
#else
#define isatty(x)       (x==0)  /* assume stdin is a tty */
#endif


#define DEBUG	0

static void testC (void)
{
#if DEBUG
#define getnum(s)	((*s++) - '0')
#define getname(s)	(nome[0] = *s++, nome)

  static int locks[10];
  lua_Object reg[10];
  char nome[2];
  char *s = luaL_check_string(1);
  nome[1] = 0;
  while (1) {
    switch (*s++) {
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
        lua_pushnumber(*(s-1) - '0');
        break;

      case 'c': reg[getnum(s)] = lua_createtable(); break;

      case 'P': reg[getnum(s)] = lua_pop(); break;

      case 'g': { int n = getnum(s); reg[n] = lua_getglobal(getname(s)); break; }

      case 'G': { int n = getnum(s);
                  reg[n] = lua_rawgetglobal(getname(s));
                  break;
                }

      case 'l': locks[getnum(s)] = lua_ref(1); break;
      case 'L': locks[getnum(s)] = lua_ref(0); break;

      case 'r': { int n = getnum(s); reg[n] = lua_getref(locks[getnum(s)]); break; }

      case 'u': lua_unref(locks[getnum(s)]); break;

      case 'p': { int n = getnum(s); reg[n] = lua_getparam(getnum(s)); break; }

      case '=': lua_setglobal(getname(s)); break;

      case 's': lua_pushstring(getname(s)); break;

      case 'o': lua_pushobject(reg[getnum(s)]); break;

      case 'f': lua_call(getname(s)); break;

      case 'i': reg[getnum(s)] = lua_gettable(); break;

      case 'I': reg[getnum(s)] = lua_rawgettable(); break;

      case 't': lua_settable(); break;

      case 'T': lua_rawsettable(); break;

      default: luaL_verror("unknown command in `testC': %c", *(s-1));

    }
  if (*s == 0) return;
  if (*s++ != ' ') lua_error("missing ` ' between commands in `testC'");
  }
#else
  lua_error("`testC' not active");
#endif
}


static void manual_input (void)
{
  if (isatty(0)) {
    char buffer[250];
    while (fgets(buffer, sizeof(buffer), stdin) != 0) {
      lua_beginblock();
      lua_dostring(buffer);
      lua_endblock();
    }
  }
  else
    lua_dofile(NULL);  /* executes stdin as a file */
}


int main (int argc, char *argv[])
{
  int i;
  int result = 0;
  iolib_open ();
  strlib_open ();
  mathlib_open ();
  lua_register("testC", testC);
  if (argc < 2)
    manual_input();
  else for (i=1; i<argc; i++) {
    if (strcmp(argv[i], "-") == 0)
      manual_input();
    else if (strcmp(argv[i], "-v") == 0)
      printf("%s  %s\n(written by %s)\n\n",
             LUA_VERSION, LUA_COPYRIGHT, LUA_AUTHORS);
    else if ((strcmp(argv[i], "-e") == 0 && i++) || strchr(argv[i], '=')) {
      if (lua_dostring(argv[i]) != 0) {
        fprintf(stderr, "lua: error running argument `%s'\n", argv[i]);
        return 1;
      }
    }
    else {
      result = lua_dofile (argv[i]);
      if (result) {
        if (result == 2) {
          fprintf(stderr, "lua: cannot execute file ");
          perror(argv[i]);
        }
        return 1;
      }
    }
  }
  return result;
}

