/*
** fallback.c
** TecCGraf - PUC-Rio
*/
 
char *rcs_fallback="$Id: fallback.c,v 1.25 1996/04/25 14:10:00 roberto Exp $";

#include <stdio.h>
#include <string.h>
 
#include "mem.h"
#include "fallback.h"
#include "opcode.h"
#include "lua.h"
#include "table.h"


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
{"error", {LUA_T_CFUNCTION, {errorFB}}, 1, 0},
{"index", {LUA_T_CFUNCTION, {indexFB}}, 2, 1},
{"gettable", {LUA_T_CFUNCTION, {gettableFB}}, 2, 1},
{"arith", {LUA_T_CFUNCTION, {arithFB}}, 3, 1},
{"order", {LUA_T_CFUNCTION, {orderFB}}, 3, 1},
{"concat", {LUA_T_CFUNCTION, {concatFB}}, 2, 1},
{"settable", {LUA_T_CFUNCTION, {gettableFB}}, 3, 0},
{"gc", {LUA_T_CFUNCTION, {GDFB}}, 1, 0},
{"function", {LUA_T_CFUNCTION, {funcFB}}, -1, -1},
                                /* no fixed number of params or results */
{"getglobal", {LUA_T_CFUNCTION, {indexFB}}, 1, 1}
                                /* same default behavior of index FB */
};

#define N_FB  (sizeof(luaI_fallBacks)/sizeof(struct FB))

void luaI_setfallback (void)
{
  int i;
  char *name = lua_getstring(lua_getparam(1));
  lua_Object func = lua_getparam(2);
  if (name == NULL || !lua_isfunction(func))
    lua_error("incorrect argument to function `setfallback'");
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
  lua_error("incorrect argument to function `setfallback'");
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
  lua_error("indexed expression not a table");
}
 

static void arithFB (void)
{
  lua_error("unexpected type at conversion to number");
}

static void concatFB (void)
{
  lua_error("unexpected type at conversion to string");
}


static void orderFB (void)
{
  lua_error("unexpected type at comparison");
}

static void GDFB (void) { }

static void funcFB (void)
{
  lua_error("call expression not a function");
}


/*
** Reference routines
*/

static struct ref {
  Object o;
  enum {LOCK, HOLD, FREE, COLLECTED} status;
} *refArray = NULL;
static int refSize = 0;

int luaI_ref (Object *object, int lock)
{
  int i;
  int oldSize;
  if (tag(object) == LUA_T_NIL)
    return -1;   /* special ref for nil */
  for (i=0; i<refSize; i++)
    if (refArray[i].status == FREE)
      goto found;
  /* no more empty spaces */
  oldSize = refSize;
  refSize = growvector(&refArray, refSize, struct ref, refEM, MAX_WORD);
  for (i=oldSize; i<refSize; i++)
    refArray[i].status = FREE;
  i = oldSize;
 found:
  refArray[i].o = *object;
  refArray[i].status = lock ? LOCK : HOLD;
  return i;
}


void lua_unref (int ref)
{
  if (ref >= 0 && ref < refSize)
    refArray[ref].status = FREE;
}


Object *luaI_getref (int ref)
{
  static Object nul = {LUA_T_NIL, {0}};
  if (ref == -1)
    return &nul;
  if (ref >= 0 && ref < refSize &&
      (refArray[ref].status == LOCK || refArray[ref].status == HOLD))
    return &refArray[ref].o;
  else
    return NULL;
}


void luaI_travlock (int (*fn)(Object *))
{
  int i;
  for (i=0; i<refSize; i++)
    if (refArray[i].status == LOCK)
      fn(&refArray[i].o);
}


void luaI_invalidaterefs (void)
{
  int i;
  for (i=0; i<refSize; i++)
    if (refArray[i].status == HOLD && !luaI_ismarked(&refArray[i].o))
      refArray[i].status = COLLECTED;
}

char *luaI_travfallbacks (int (*fn)(Object *))
{
  int i;
  for (i=0; i<N_FB; i++)
    if (fn(&luaI_fallBacks[i].function))
      return luaI_fallBacks[i].kind;
  return NULL;
}
