/*
** fallback.c
** TecCGraf - PUC-Rio
*/
 
char *rcs_fallback="$Id: fallback.c,v 1.30 1997/03/20 19:20:43 roberto Exp roberto $";

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


static char *typenames[] = { /* ORDER LUA_T */
  "userdata", "line", "cmark", "mark", "function",
  "function", "table", "string", "number", "nil",
  NULL
};


void luaI_type (void)
{
  lua_Object o = lua_getparam(1);
  lua_pushstring(typenames[-ttype(luaI_Address(o))]);
  lua_pushnumber(lua_tag(o));
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

char *luaI_eventname[] = {  /* ORDER IM */
  "gettable", "settable", "index", "add", "sub", "mul", "div",
  "pow", "unm", "lt", "le", "gt", "ge", "concat", "gc", "function",
  NULL
};


static char *geventname[] = {  /* ORDER GIM */
  "error", "getglobal", "setglobal",
  NULL
};

static int findstring (char *name, char *list[])
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
  int e = findstring(name, list);
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

static char validevents[NUM_TYPES][IM_N] = { /* ORDER LUA_T, ORDER IM */
{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},  /* LUA_T_USERDATA */
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  /* LUA_T_LINE */
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  /* LUA_T_CMARK */
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  /* LUA_T_MARK */
{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0},  /* LUA_T_CFUNCTION */
{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0},  /* LUA_T_FUNCTION */
{0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},  /* LUA_T_ARRAY */
{1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},  /* LUA_T_STRING */
{1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1},  /* LUA_T_NUMBER */
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0}   /* LUA_T_NIL */
};

static int validevent (lua_Type t, int e)
{
  return (t < LUA_T_NIL) ?  1 : validevents[-t][e];
}


static void init_entry (int tag)
{
  int i;
  for (i=0; i<IM_N; i++)
    luaI_IMtable[-tag].int_method[i].ttype = LUA_T_NIL;
}

void luaI_initfallbacks (void)
{
  if (luaI_IMtable == NULL) {
    int i;
    IMtable_size = NUM_TYPES+10;
    luaI_IMtable = newvector(IMtable_size, struct IM);
    for (i=LUA_T_NIL; i<=LUA_T_USERDATA; i++) {
      luaI_IMtable[-i].tp = (lua_Type)i;
      init_entry(i);
    }
  }
}

int lua_newtag (char *t)
{
  int tp;
  --last_tag;
  if ((-last_tag) >= IMtable_size) {
    luaI_initfallbacks();
    IMtable_size = growvector(&luaI_IMtable, IMtable_size,
                              struct IM, memEM, MAX_INT);
  }
  tp = -findstring(t, typenames);
  if (tp == LUA_T_ARRAY || tp == LUA_T_USERDATA)
    luaI_IMtable[-last_tag].tp = tp;
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

Object *luaI_getim (int tag, IMS event)
{
  if (tag > LUA_T_USERDATA)
    tag = LUA_T_USERDATA;  /* default for non-registered tags */
  return &luaI_IMtable[-tag].int_method[event];
}

Object *luaI_getimbyObj (Object *o, IMS event)
{
  return luaI_getim(luaI_tag(o), event);
}

void luaI_setintmethod (void)
{
  int t = (int)luaL_check_number(1, "setintmethod");
  int e = luaI_checkevent(luaL_check_string(2, "setintmethod"), luaI_eventname);
  lua_Object func = lua_getparam(3);
  checktag(t);
  if (!validevent(t, e))
    lua_error("cannot change this internal method");
  luaL_arg_check(lua_isnil(func) || lua_isfunction(func), "setintmethod",
                 3, "function expected");
  luaI_pushobject(&luaI_IMtable[-t].int_method[e]);
  luaI_IMtable[-t].int_method[e] = *luaI_Address(func);
}

static Object gmethod[GIM_N] = {
  {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}
};

Object *luaI_getgim (IMGS event)
{
  return &gmethod[event];
}

void luaI_setglobalmethod (void)
{
  int e = luaI_checkevent(luaL_check_string(1, "setintmethod"), geventname);
  lua_Object func = lua_getparam(2);
  luaL_arg_check(lua_isnil(func) || lua_isfunction(func), "setintmethod",
                 2, "function expected");
  luaI_pushobject(&gmethod[e]);
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


static void fillvalids (IMS e, Object *func)
{
  int t;
  for (t=LUA_T_NIL; t<=LUA_T_USERDATA; t++)
    if (validevent(t, e))
      luaI_IMtable[-t].int_method[e] = *func;
}

void luaI_setfallback (void)
{
  int e;
  Object oldfunc;
  lua_CFunction replace;
  char *name = luaL_check_string(1, "setfallback");
  lua_Object func = lua_getparam(2);
  luaL_arg_check(lua_isfunction(func), "setfallback", 2, "function expected");
  e = findstring(name, geventname);
  if (e >= 0) {  /* global event */
    oldfunc = gmethod[e];
    gmethod[e] = *luaI_Address(func);
    replace = (e == GIM_ERROR) ? errorFB : nilFB;
  }
  else if ((e = findstring(name, luaI_eventname)) >= 0) {
    oldfunc = luaI_IMtable[LUA_T_USERDATA].int_method[e];
    fillvalids(e, luaI_Address(func));
    replace = (e == IM_GC || e == IM_INDEX) ? nilFB : typeFB;
  }
  else if (strcmp(name, "arith") == 0) {  /* old arith fallback */
    int i;
    oldfunc = luaI_IMtable[LUA_T_USERDATA].int_method[IM_ADD];
    for (i=IM_ADD; i<=IM_UNM; i++)  /* ORDER IM */
      fillvalids(i, luaI_Address(func));
    replace = typeFB;
  }
  else if (strcmp(name, "order") == 0) {  /* old order fallback */
    int i;
    oldfunc = luaI_IMtable[LUA_T_USERDATA].int_method[IM_LT];
    for (i=IM_LT; i<=IM_GE; i++)  /* ORDER IM */
      fillvalids(i, luaI_Address(func));
    replace = typeFB;
  }
  else {
    lua_error("invalid fallback name");
    replace = NULL;  /* to avoid warnings */
  }
  if (oldfunc.ttype != LUA_T_NIL)
    luaI_pushobject(&oldfunc);
  else
    lua_pushcfunction(replace);
}
 
