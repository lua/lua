/*
** fallback.c
** TecCGraf - PUC-Rio
*/
 
char *rcs_fallback="$Id: fallback.c,v 1.1 1994/11/07 15:20:56 roberto Exp roberto $";

#include <stdio.h>
#include <stdlib.h>
 
#include "fallback.h"
#include "opcode.h"
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


/*
** Lock routines
*/

static Object *lockArray = NULL;
static int lockSize = 0;

int lua_lock (lua_Object object)
{
  int i;
  int oldSize;
  if (lua_isnil(object))
    return -1;
  for (i=0; i<lockSize; i++)
    if (tag(&lockArray[i]) == LUA_T_NIL)
    {
      lockArray[i] = *luaI_Address(object);
      return i;
    }
  /* no more empty spaces */
  oldSize = lockSize;
  if (lockArray == NULL)
  {
    lockSize = 10;
    lockArray = (Object *)malloc(lockSize);
  }
  else
  {
    lockSize = 3*oldSize/2 + 5;
    lockArray = (Object *)realloc(lockArray, lockSize);
  }
  if (lockArray == NULL)
  {
    lockSize = 0;
    lua_error("lock - not enough memory");
  }
  for (i=oldSize; i<lockSize; i++)
    tag(&lockArray[i]) = LUA_T_NIL;
  lockArray[oldSize] = *luaI_Address(object);
  return oldSize;
}


void lua_unlock (int ref)
{
  tag(&lockArray[ref]) = LUA_T_NIL;
}


Object *luaI_getlocked (int ref)
{
  return &lockArray[ref];
}


void luaI_travlock (void (*fn)(Object *))
{
  int i;
  for (i=0; i<lockSize; i++)
    fn(&lockArray[i]);
}

