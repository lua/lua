/*
** fallback.c
** TecCGraf - PUC-Rio
*/
 
char *rcs_fallback="$Id: fallback.c,v 1.3 1994/11/09 18:12:42 roberto Exp roberto $";

#include <stdio.h>
#include <stdlib.h>
 
#include "fallback.h"
#include "opcode.h"
#include "inout.h"
#include "lua.h"


/*
** Warning: This list must be in the same order as the #define's
*/
struct FB  luaI_fallBacks[] = {
{"error", {LUA_T_CFUNCTION, luaI_errorFB}},
{"index", {LUA_T_CFUNCTION, luaI_indexFB}},
{"gettable", {LUA_T_CFUNCTION, luaI_gettableFB}},
{"arith", {LUA_T_CFUNCTION, luaI_arithFB}},
{"order", {LUA_T_CFUNCTION, luaI_orderFB}},
{"concat", {LUA_T_CFUNCTION, luaI_concatFB}},
{"unminus", {LUA_T_CFUNCTION, luaI_arithFB}},
{"settable", {LUA_T_CFUNCTION, luaI_gettableFB}}
};

#define N_FB  (sizeof(luaI_fallBacks)/sizeof(struct FB))

void luaI_setfallback (void)
{
  int i;
  char *name = lua_getstring(lua_getparam(1));
  lua_Object func = lua_getparam(2);
  if (name == NULL || !(lua_isfunction(func) || lua_iscfunction(func)))
  {
    lua_pushnil();
    return;
  }
  for (i=0; i<N_FB; i++)
  {
    if (strcmp(luaI_fallBacks[i].kind, name) == 0)
    {
      luaI_pushobject(&luaI_fallBacks[i].function);
      luaI_fallBacks[i].function = *luaI_Address(func);
      return;
    }
  }
  /* name not found */
  lua_pushnil();
}


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
  lua_reportbug("indexed expression not a table");
}
 

void luaI_arithFB (void)
{
  lua_reportbug("unexpected type at conversion to number");
}

void luaI_concatFB (void)
{
  lua_reportbug("unexpected type at conversion to string");
}


void luaI_orderFB (void)
{
  lua_reportbug("unexpected type at comparison");
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

