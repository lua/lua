/*
** fallback.c
** TecCGraf - PUC-Rio
*/
 
char *rcs_fallback="$Id: fallback.c,v 1.25 1996/04/25 14:10:00 roberto Exp roberto $";

#include <stdio.h>
#include <string.h>
 
#include "mem.h"
#include "fallback.h"
#include "opcode.h"
#include "lua.h"
#include "table.h"
#include "tree.h"
#include "hash.h"


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
{"gettable", {LUA_T_CFUNCTION, {gettableFB}}, 2, 1},
{"arith", {LUA_T_CFUNCTION, {arithFB}}, 3, 1},
{"order", {LUA_T_CFUNCTION, {orderFB}}, 3, 1},
{"concat", {LUA_T_CFUNCTION, {concatFB}}, 2, 1},
{"settable", {LUA_T_CFUNCTION, {gettableFB}}, 3, 0},
{"gc", {LUA_T_CFUNCTION, {GDFB}}, 1, 0},
{"function", {LUA_T_CFUNCTION, {funcFB}}, -1, -1},
                                /* no fixed number of params or results */
{"getglobal", {LUA_T_CFUNCTION, {indexFB}}, 1, 1},
                                /* same default behavior of index FB */
{"index", {LUA_T_CFUNCTION, {indexFB}}, 2, 1},
{"error", {LUA_T_CFUNCTION, {errorFB}}, 1, 0}
};

#define N_FB  (sizeof(luaI_fallBacks)/sizeof(struct FB))

static int luaI_findevent (char *name)
{
  int i;
  for (i=0; i<N_FB; i++)
    if (strcmp(luaI_fallBacks[i].kind, name) == 0)
      return i;
  /* name not found */
  lua_error("invalid event name");
  return 0;  /* to avoid warnings */
}


void luaI_setfallback (void)
{
  int i;
  char *name = lua_getstring(lua_getparam(1));
  lua_Object func = lua_getparam(2);
  if (name == NULL || !lua_isfunction(func))
    lua_error("incorrect argument to function `setfallback'");
  i = luaI_findevent(name);
  luaI_pushobject(&luaI_fallBacks[i].function);
  luaI_fallBacks[i].function = *luaI_Address(func);
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


/* -------------------------------------------
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


/* -------------------------------------------
* Internal Methods 
*/
#define BASE_TAG 1000

static struct IM {
  lua_Type tp;
  Object int_method[FB_N];
 } *luaI_IMtable = NULL;
static int IMtable_size = 0;
static int last_tag = BASE_TAG-1;

int lua_newtag (char *t)
{
  int i;
  ++last_tag;
  if ((last_tag-BASE_TAG) >= IMtable_size)
    IMtable_size = growvector(&luaI_IMtable, IMtable_size,
                              struct IM, memEM, MAX_INT);
  if (strcmp(t, "table") == 0)
    luaI_IMtable[last_tag-BASE_TAG].tp = LUA_T_ARRAY;
  else if (strcmp(t, "userdata") == 0)
    luaI_IMtable[last_tag-BASE_TAG].tp = LUA_T_USERDATA;
  else
    lua_error("invalid type for new tag");
  for (i=0; i<FB_N; i++)
    luaI_IMtable[last_tag-BASE_TAG].int_method[i].tag = LUA_T_NIL;
  return last_tag;
}

static int validtag (int tag)
{
  return (BASE_TAG <= tag && tag <= last_tag);
}

static void checktag (int tag)
{
  if (!validtag(tag))
    lua_error("invalid tag");
}

void luaI_settag (int tag, Object *o)
{
  checktag(tag);
  if (tag(o) != luaI_IMtable[tag-BASE_TAG].tp)
    lua_error("Tag is not compatible with this type");
  if (o->tag == LUA_T_ARRAY)
    o->value.a->htag = tag;
  else  /* must be userdata */
    o->value.ts->tag = tag;
}

int luaI_tag (Object *o)
{
  lua_Type t = tag(o);
  if (t == LUA_T_USERDATA)
    return o->value.ts->tag;
  else if (t == LUA_T_ARRAY)
    return o->value.a->htag;
  else return t;
}

Object *luaI_getim (int tag, int event)
{
  if (tag == 0)
    return &luaI_fallBacks[event].function;
  else if (validtag(tag)) {
    Object *func = &luaI_IMtable[tag-BASE_TAG].int_method[event];
    if (func->tag == LUA_T_NIL)
      return NULL;
    else
      return func;
  }
  else return NULL;
}

void luaI_setintmethod (void)
{
  lua_Object tag = lua_getparam(1);
  lua_Object event = lua_getparam(2);
  lua_Object func = lua_getparam(3);
  if (!(lua_isnumber(tag) && lua_isstring(event) && lua_isfunction(func)))
    lua_error("incorrect arguments to function `setintmethod'");
  else {
    int i = luaI_findevent(lua_getstring(event));
    int t = lua_getnumber(tag);
    checktag(t);
    luaI_IMtable[t-BASE_TAG].int_method[i] = *luaI_Address(func);
  }
}
