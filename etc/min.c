/*
* min.c -- a minimal Lua interpreter
* only dynamic loading is enabled -- all libraries must be dynamically loaded
* no interaction, only batch execution
*/

#include <stdio.h>
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

static int run(lua_State *L)
{
 char **argv=lua_touserdata(L,1);
 lua_register(L,"error",lua_error);
 luaopen_loadlib(L);
 while (*++argv)
 {
  if (luaL_loadfile(L,*argv)) lua_error(L); else lua_call(L,0,0);
 }
 return 0;
}

#define report(s) fprintf(stderr,"%s\n",s)

int main(int argc, char *argv[])
{
 lua_State *L=lua_open();
 if (L==NULL)
 {
  report("not enough memory for state");
  return 1;
 }
 if (lua_cpcall(L,run,argv)) report(lua_tostring(L,-1));
 lua_close(L);
 return 0;
}
