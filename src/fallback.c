/*
** fallback.c
** TecCGraf - PUC-Rio
*/
 
char *rcs_fallback="$Id: fallback.c,v 1.11 1995/02/06 19:34:03 roberto Exp $";

#include <stdio.h>
#include <string.h>
 
#include "mem.h"
#include "fallback.h"
#include "opcode.h"
#include "inout.h"
#include "lua.h"


static void errorFB (void);
static void indexFB (void);
static void gettableFB (void);
static void arithFB (void);
static void concatFB (void);
static void orderFB (void);
static void GDFB (void);
static void funcFB (void);


/*
** Warning: This list must be in the same order as the #define's
*/
struct FB  luaI_fallBacks[] = {
{"error", {LUA_T_CFUNCTION, errorFB}},
{"index", {LUA_T_CFUNCTION, indexFB}},
{"gettable", {LUA_T_CFUNCTION, gettableFB}},
{"arith", {LUA_T_CFUNCTION, arithFB}},
{"order", {LUA_T_CFUNCTION, orderFB}},
{"concat", {LUA_T_CFUNCTION, concatFB}},
{"settable", {LUA_T_CFUNCTION, gettableFB}},
{"gc", {LUA_T_CFUNCTION, GDFB}},
{"function", {LUA_T_CFUNCTION, funcFB}}
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


static void errorFB (void)
{
  lua_Object o = lua_getparam(1);
  if (lua_isstring(o))
    fprintf (stderr, "lua: %s\n", lua_getstring(o));
  else
    fprintf(stderr, "lua: unknown error\n");
}
 

static void indexFB (void)
{
  lua_pushnil();
}
 

static void gettableFB (void)
{
  lua_reportbug("indexed expression not a table");
}
 

static void arithFB (void)
{
  lua_reportbug("unexpected type at conversion to number");
}

static void concatFB (void)
{
  lua_reportbug("unexpected type at conversion to string");
}


static void orderFB (void)
{
  lua_reportbug("unexpected type at comparison");
}

static void GDFB (void) { }

static void funcFB (void)
{
  lua_reportbug("call expression not a function");
}


/*
** Lock routines
*/

static Object *lockArray = NULL;
static Word lockSize = 0;

int luaI_lock (Object *object)
{
  Word i;
  Word oldSize;
  if (tag(object) == LUA_T_NIL)
    return -1;
  for (i=0; i<lockSize; i++)
    if (tag(&lockArray[i]) == LUA_T_NIL)
    {
      lockArray[i] = *object;
      return i;
    }
  /* no more empty spaces */
  oldSize = lockSize;
  if (lockArray == NULL)
  {
    lockSize = 10;
    lockArray = newvector(lockSize, Object);
  }
  else
  {
    lockSize = 3*oldSize/2 + 5;
    lockArray = growvector(lockArray, lockSize, Object);
  }
  for (i=oldSize; i<lockSize; i++)
    tag(&lockArray[i]) = LUA_T_NIL;
  lockArray[oldSize] = *object;
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
  Word i;
  for (i=0; i<lockSize; i++)
    fn(&lockArray[i]);
}

