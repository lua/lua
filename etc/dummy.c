/*
* dummy.c -- a minimal Lua library for testing dynamic loading
*/

#include <stdio.h>
#include "lua.h"

int luaopen_dummy (lua_State *L) {
 puts("Hello from dummy");
 return 0;
}
