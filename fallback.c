/*
** fallback.c
** TecCGraf - PUC-Rio
*/
 
char *rcs_fallback="$Id: fallback.c,v 1.27 1997/03/11 18:44:28 roberto Exp roberto $";

#include <stdio.h>
#include <string.h>
 
#include "auxlib.h"
#include "mem.h"
#include "fallback.h"
#include "opcode.h"
#include "lua.h"
#include "table.h"
#include "tree.h"
#include "hash.h"




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
  if (ttype(object) == LUA_T_NIL)
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


/* -------------------------------------------
* Internal Methods 
*/

char *eventname[] = {
  "gettable",  /* IM_GETTABLE */
  "arith",  /* IM_ARITH */
  "order",  /* IM_ORDER */
  "concat",  /* IM_CONCAT */
  "settable",  /* IM_SETTABLE */
  "gc",  /* IM_GC */
  "function",  /* IM_FUNCTION */
  "index",  /* IM_INDEX */
  NULL
};


char *geventname[] = {
  "error",  /* GIM_ERROR */
  "getglobal",  /* GIM_GETGLOBAL */
  "setglobal",  /* GIM_SETGLOBAL */
  NULL
};

static int luaI_findevent (char *name, char *list[])
{
  int i;
  for (i=0; list[i]; i++)
    if (strcmp(list[i], name) == 0)
      return i;
  /* name not found */
  return -1;
}

static int luaI_checkevent (char *name, char *list[])
{
  int e = luaI_findevent(name, list);
  if (e < 0)
    lua_error("invalid event name");
  return e;
}


static struct IM {
  lua_Type tp;
  Object int_method[IM_N];
} *luaI_IMtable = NULL;

static int IMtable_size = 0;
static int last_tag = LUA_T_NIL;

static struct {
  lua_Type t;
  int event;
} exceptions[] = {  /* list of events that cannot be modified */
    {LUA_T_NUMBER, IM_ARITH},
    {LUA_T_NUMBER, IM_ORDER},
    {LUA_T_NUMBER, IM_GC},
    {LUA_T_STRING, IM_ARITH},
    {LUA_T_STRING, IM_ORDER},
    {LUA_T_STRING, IM_CONCAT},
    {LUA_T_STRING, IM_GC},
    {LUA_T_ARRAY, IM_GETTABLE},
    {LUA_T_ARRAY, IM_SETTABLE},
    {LUA_T_FUNCTION, IM_FUNCTION},
    {LUA_T_FUNCTION, IM_GC},
    {LUA_T_CFUNCTION, IM_FUNCTION},
    {LUA_T_CFUNCTION, IM_GC},
    {LUA_T_NIL, 0}  /* flag end of list */
};


static int validevent (int t, int event)
{
  int i;
  if (t == LUA_T_NIL)  /* cannot modify any event for nil */
    return 0;
  for (i=0; exceptions[i].t != LUA_T_NIL; i++)
    if (exceptions[i].t == t && exceptions[i].event == event)
      return 0;
  return 1;
}

static void init_entry (int tag)
{
  int i;
  for (i=0; i<IM_N; i++)
    luaI_IMtable[-tag].int_method[i].ttype = LUA_T_NIL;
}

void luaI_initfallbacks (void)
{
  int i;
  IMtable_size = NUM_TYPES+10;
  luaI_IMtable = newvector(IMtable_size, struct IM);
  for (i=LUA_T_NIL; i<=LUA_T_USERDATA; i++) {
    luaI_IMtable[-i].tp = (lua_Type)i;
    init_entry(i);
  }
}

int lua_newtag (char *t)
{
  --last_tag;
  if ((-last_tag) >= IMtable_size)
    IMtable_size = growvector(&luaI_IMtable, IMtable_size,
                              struct IM, memEM, MAX_INT);
  if (strcmp(t, "table") == 0)
    luaI_IMtable[-last_tag].tp = LUA_T_ARRAY;
  else if (strcmp(t, "userdata") == 0)
    luaI_IMtable[-last_tag].tp = LUA_T_USERDATA;
  else
    lua_error("invalid type for new tag");
  init_entry(last_tag);
  return last_tag;
}


#define validtag(tag)  (last_tag <= (tag) && (tag) <= 0)


static void checktag (int tag)
{
  if (!validtag(tag))
    lua_error("invalid tag");
}

lua_Type luaI_typetag (int tag)
{
  if (tag >= 0) return LUA_T_USERDATA;
  else {
    checktag(tag);
    return luaI_IMtable[-tag].tp;
  }
}

void luaI_settag (int tag, Object *o)
{
  if (ttype(o) != luaI_typetag(tag))
    lua_error("Tag is not compatible with this type");
  if (o->ttype == LUA_T_ARRAY)
    o->value.a->htag = tag;
  else  /* must be userdata */
    o->value.ts->tag = tag;
}

int luaI_tag (Object *o)
{
  lua_Type t = ttype(o);
  if (t == LUA_T_USERDATA)
    return o->value.ts->tag;
  else if (t == LUA_T_ARRAY)
    return o->value.a->htag;
  else return t;
}

Object *luaI_getim (int tag, int event)
{
  if (tag > LUA_T_USERDATA)
    tag = LUA_T_USERDATA;  /* default for non-registered tags */
  return &luaI_IMtable[-tag].int_method[event];
}

Object *luaI_getimbyObj (Object *o, int event)
{
  return luaI_getim(luaI_tag(o), event);
}

void luaI_setintmethod (void)
{
  int t = (int)luaL_check_number(1, "setintmethod");
  int e = luaI_checkevent(luaL_check_string(2, "setintmethod"), eventname);
  lua_Object func = lua_getparam(3);
  if (!validevent(t, e))
    lua_error("cannot change this internal method");
  luaL_arg_check(lua_isnil(func) || lua_isfunction(func), "setintmethod",
                 3, "function expected");
  checktag(t);
  luaI_IMtable[-t].int_method[e] = *luaI_Address(func);
}

static Object gmethod[GIM_N] = {
  {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}
};

Object *luaI_getgim (int event)
{
  return &gmethod[event];
}

void luaI_setglobalmethod (void)
{
  int e = luaI_checkevent(luaL_check_string(1, "setintmethod"), geventname);
  lua_Object func = lua_getparam(2);
  luaL_arg_check(lua_isnil(func) || lua_isfunction(func), "setintmethod",
                 2, "function expected");
  gmethod[e] = *luaI_Address(func);
}

char *luaI_travfallbacks (int (*fn)(Object *))
{ /* ??????????
  int i;
  for (i=0; i<N_FB; i++)
    if (fn(&luaI_fallBacks[i].function))
      return luaI_fallBacks[i].kind; */
  return NULL;
}


/*
* ===================================================================
* compatibility with old fallback system
*/


static void errorFB (void)
{
  lua_Object o = lua_getparam(1);
  if (lua_isstring(o))
    fprintf (stderr, "lua: %s\n", lua_getstring(o));
  else
    fprintf(stderr, "lua: unknown error\n");
}
 

static void nilFB (void) { }
 

static void typeFB (void)
{
  lua_error("unexpected type");
}


void luaI_setfallback (void)
{
  int e;
  char *name = luaL_check_string(1, "setfallback");
  lua_Object func = lua_getparam(2);
  luaL_arg_check(lua_isfunction(func), "setfallback", 2, "function expected");
  e = luaI_findevent(name, geventname);
  if (e >= 0) {  /* global event */
    switch (e) {
      case GIM_ERROR:
        gmethod[e] = *luaI_Address(func);
        lua_pushcfunction(errorFB);
        break;
      case GIM_GETGLOBAL:  /* goes through */
      case GIM_SETGLOBAL:
        gmethod[e] = *luaI_Address(func);
        lua_pushcfunction(nilFB);
        break;
      default: lua_error("internal error");
    }
  }
  else {  /* tagged name? */
    int t;
    Object oldfunc;
    e = luaI_checkevent(name, eventname);
    oldfunc = luaI_IMtable[LUA_T_USERDATA].int_method[e];
    for (t=LUA_T_NIL; t<=LUA_T_USERDATA; t++)
      if (validevent(t, e))
        luaI_IMtable[-t].int_method[e] = *luaI_Address(func);
    if (oldfunc.ttype != LUA_T_NIL)
      luaI_pushobject(&oldfunc);
    else {
      switch (e) {
       case IM_GC:  case IM_INDEX:
         lua_pushcfunction(nilFB);
         break;
       default:
         lua_pushcfunction(typeFB);
         break;
      }
    }
  }
}
 
