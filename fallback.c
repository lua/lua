/*
** fallback.c
** TecCGraf - PUC-Rio
*/
 
char *rcs_fallback="$Id: $";

#include <stdio.h>
 
#include "fallback.h"
#include "lua.h"


void luaI_errorFB (void)
{
  lua_Object o = lua_getparam(1);
  if (lua_isstring(o))
    fprintf (stderr, "lua: %s\n", lua_getstring(o));
  else
    fprintf(stderr, "lua: unknown error\n");
}
 

void luaI_indexFB (void)
{
  lua_pushnil();
}
 

void luaI_gettableFB (void)
{
  lua_error("indexed expression not a table");
}
 

void luaI_arithFB (void)
{
  lua_error("unexpected type at conversion to number");
}

void luaI_concatFB (void)
{
  lua_error("unexpected type at conversion to string");
}


void luaI_orderFB (void)
{
  lua_error("unexpected type at comparison");
}

