/*
* min.c
* a minimal Lua interpreter. loads stdin only.
* no standard library, only builtin functions.
*/

#include "lua.h"

int main(void)
{
 lua_open();
 return lua_dofile(0);
}
