/*
** fallback.c
** TecCGraf - PUC-Rio
*/
 
char *rcs_fallback="$Id: fallback.c,v 2.9 1997/06/23 18:27:53 roberto Exp $";

#include <stdio.h>
#include <string.h>
 
#include "auxlib.h"
#include "luamem.h"
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
  TObject o;
  enum {LOCK, HOLD, FREE, COLLECTED} status;
} *refArray = NULL;
static int refSize = 0;

int luaI_ref (TObject *object, int lock)
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


TObject *luaI_getref (int ref)
{
  static TObject nul = {LUA_T_NIL, {0}};
  if (ref == -1)
    return &nul;
  if (ref >= 0 && ref < refSize &&
      (refArray[ref].status == LOCK || refArray[ref].status == HOLD))
    return &refArray[ref].o;
  else
    return NULL;
}


void luaI_travlock (int (*fn)(TObject *))
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
  "gettable", "settable", "index", "getglobal", "setglobal", "add",
  "sub", "mul", "div", "pow", "unm", "lt", "le", "gt", "ge",
  "concat", "gc", "function",
  NULL
};



static int luaI_checkevent (char *name, char *list[])
{
  int e = luaI_findstring(name, list);
  if (e < 0)
    luaL_verror("`%s' is not a valid event name", name);
  return e;
}


struct IM *luaI_IMtable = NULL;

static int IMtable_size = 0;
static int last_tag = LUA_T_NIL;  /* ORDER LUA_T */


/* events in LUA_T_LINE are all allowed, since this is used as a
*  'placeholder' for "default" fallbacks
*/
static char validevents[NUM_TYPES][IM_N] = { /* ORDER LUA_T, ORDER IM */
{1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1},  /* LUA_T_USERDATA */
{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},  /* LUA_T_LINE */
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  /* LUA_T_CMARK */
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  /* LUA_T_MARK */
{1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0},  /* LUA_T_CFUNCTION */
{1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0},  /* LUA_T_FUNCTION */
{0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},  /* LUA_T_ARRAY */
{1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},  /* LUA_T_STRING */
{1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1},  /* LUA_T_NUMBER */
{1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}   /* LUA_T_NIL */
};

static int validevent (lua_Type t, int e)
{ /* ORDER LUA_T */
  return (t < LUA_T_NIL) ?  1 : validevents[-t][e];
}


static void init_entry (int tag)
{
  int i;
  for (i=0; i<IM_N; i++)
    ttype(luaI_getim(tag, i)) = LUA_T_NIL;
}

void luaI_initfallbacks (void)
{
  if (luaI_IMtable == NULL) {
    int i;
    IMtable_size = NUM_TYPES+10;
    luaI_IMtable = newvector(IMtable_size, struct IM);
    for (i=LUA_T_NIL; i<=LUA_T_USERDATA; i++)
      init_entry(i);
  }
}

int lua_newtag (void)
{
  --last_tag;
  if ((-last_tag) >= IMtable_size) {
    luaI_initfallbacks();
    IMtable_size = growvector(&luaI_IMtable, IMtable_size,
                              struct IM, memEM, MAX_INT);
  }
  init_entry(last_tag);
  return last_tag;
}


static void checktag (int tag)
{
  if (!(last_tag <= tag && tag <= 0))
    luaL_verror("%d is not a valid tag", tag);
}

void luaI_realtag (int tag)
{
  if (!(last_tag <= tag && tag < LUA_T_NIL))
    luaL_verror("tag %d is not result of `newtag'", tag);
}


void luaI_settag (int tag, TObject *o)
{
  luaI_realtag(tag);
  switch (ttype(o)) {
    case LUA_T_ARRAY:
      o->value.a->htag = tag;
      break;
    case LUA_T_USERDATA:
      o->value.ts->tag = tag;
      break;
    default:
      luaL_verror("cannot change the tag of a %s", luaI_typenames[-ttype(o)]);
  }
}


int luaI_efectivetag (TObject *o)
{
  lua_Type t = ttype(o);
  if (t == LUA_T_USERDATA) {
    int tag = o->value.ts->tag;
    return (tag >= 0) ? LUA_T_USERDATA : tag;
  }
  else if (t == LUA_T_ARRAY)
    return o->value.a->htag;
  else return t;
}


void luaI_gettagmethod (void)
{
  int t = (int)luaL_check_number(1);
  int e = luaI_checkevent(luaL_check_string(2), luaI_eventname);
  checktag(t);
  if (validevent(t, e))
    luaI_pushobject(luaI_getim(t,e));
}


void luaI_settagmethod (void)
{
  int t = (int)luaL_check_number(1);
  int e = luaI_checkevent(luaL_check_string(2), luaI_eventname);
  lua_Object func = lua_getparam(3);
  checktag(t);
  if (!validevent(t, e))
    luaL_verror("cannot change internal method `%s' for tag %d",
                luaI_eventname[e], t);
  luaL_arg_check(lua_isnil(func) || lua_isfunction(func),
                 3, "function expected");
  luaI_pushobject(luaI_getim(t,e));
  *luaI_getim(t, e) = *luaI_Address(func);
}


static void stderrorim (void)
{
  lua_Object s = lua_getparam(1);
  if (lua_isstring(s))
    fprintf(stderr, "lua: %s\n", lua_getstring(s));
}

static TObject errorim = {LUA_T_CFUNCTION, {stderrorim}};


TObject *luaI_geterrorim (void)
{
  return &errorim;
}

void luaI_seterrormethod (void)
{
  lua_Object func = lua_getparam(1);
  luaL_arg_check(lua_isnil(func) || lua_isfunction(func),
                 1, "function expected");
  luaI_pushobject(&errorim);
  errorim = *luaI_Address(func);
}

char *luaI_travfallbacks (int (*fn)(TObject *))
{
  int e;
  if (fn(&errorim))
    return "error";
  for (e=IM_GETTABLE; e<=IM_FUNCTION; e++) {  /* ORDER IM */
    int t;
    for (t=0; t>=last_tag; t--)
      if (fn(luaI_getim(t,e)))
        return luaI_eventname[e];
  }
  return NULL;
}


/*
* ===================================================================
* compatibility with old fallback system
*/
#if	LUA_COMPAT2_5

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


static void fillvalids (IMS e, TObject *func)
{
  int t;
  for (t=LUA_T_NIL; t<=LUA_T_USERDATA; t++)
    if (validevent(t, e))
      *luaI_getim(t, e) = *func;
}


void luaI_setfallback (void)
{
  static char *oldnames [] = {"error", "getglobal", "arith", "order", NULL};
  TObject oldfunc;
  lua_CFunction replace;
  char *name = luaL_check_string(1);
  lua_Object func = lua_getparam(2);
  luaI_initfallbacks();
  luaL_arg_check(lua_isfunction(func), 2, "function expected");
  switch (luaI_findstring(name, oldnames)) {
    case 0:  /* old error fallback */
      oldfunc = errorim;
      errorim = *luaI_Address(func);
      replace = errorFB;
      break;
    case 1:  /* old getglobal fallback */
      oldfunc = *luaI_getim(LUA_T_NIL, IM_GETGLOBAL);
      *luaI_getim(LUA_T_NIL, IM_GETGLOBAL) = *luaI_Address(func);
      replace = nilFB;
      break;
    case 2: {  /* old arith fallback */
      int i;
      oldfunc = *luaI_getim(LUA_T_NUMBER, IM_POW);
      for (i=IM_ADD; i<=IM_UNM; i++)  /* ORDER IM */
        fillvalids(i, luaI_Address(func));
      replace = typeFB;
      break;
    }
    case 3: {  /* old order fallback */
      int i;
      oldfunc = *luaI_getim(LUA_T_LINE, IM_LT);
      for (i=IM_LT; i<=IM_GE; i++)  /* ORDER IM */
        fillvalids(i, luaI_Address(func));
      replace = typeFB;
      break;
    }
    default: {
      int e;
      if ((e = luaI_findstring(name, luaI_eventname)) >= 0) {
        oldfunc = *luaI_getim(LUA_T_LINE, e);
        fillvalids(e, luaI_Address(func));
        replace = (e == IM_GC || e == IM_INDEX) ? nilFB : typeFB;
      }
      else {
        luaL_verror("`%s' is not a valid fallback name", name);
        replace = NULL;  /* to avoid warnings */
      }
    }
  }
  if (oldfunc.ttype != LUA_T_NIL)
    luaI_pushobject(&oldfunc);
  else
    lua_pushcfunction(replace);
}
#endif 
