/*
** lua.c
** Linguagem para Usuarios de Aplicacao
** TeCGraf - PUC-Rio
** 28 Apr 93
*/

#include <stdio.h>

#include "lua.h"
#include "lualib.h"


void test (void)
{
  lua_pushobject(lua_getparam(1));
  lua_call ("c", 1);
}


static void callfunc (void)
{
 lua_Object obj = lua_getparam (1);
 if (lua_isstring(obj)) lua_call(lua_getstring(obj),0);
}

static void execstr (void)
{
 lua_Object obj = lua_getparam (1);
 if (lua_isstring(obj)) lua_dostring(lua_getstring(obj));
}

void main (int argc, char *argv[])
{
 int i;
 if (argc < 2)
 {
  puts ("usage: lua filename [functionnames]");
  return;
 }
 lua_register ("callfunc", callfunc);
 lua_register ("execstr", execstr);
 lua_register ("test", test);
 iolib_open ();
 strlib_open ();
 mathlib_open ();
 lua_dofile (argv[1]);
 for (i=2; i<argc; i++)
 {
  lua_call (argv[i],0);
 }
}


