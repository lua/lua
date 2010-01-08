/*
* min.c -- a minimal Lua interpreter
* runs one file from the command line or stdin if none given.
* minimal error handling, no traceback, no interaction, no standard library,
* only a "print" function.
*/

#include <stdio.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

static int print(lua_State *L)
{
 int n=lua_gettop(L);
 int i;
 const char *s="";
 for (i=1; i<=n; i++)
 {
  printf("%s%s",s,luaL_tolstring(L,i,NULL));
  s="\t";
 }
 printf("\n");
 return 0;
}

int main(int argc, char *argv[])
{
 lua_State *L=lua_open();
 lua_register(L,"print",print);
 luaL_openlibs(L);
 if (luaL_dofile(L,argv[1])!=0) fprintf(stderr,"%s\n",lua_tostring(L,-1));
 lua_close(L);
 return 0;
}
