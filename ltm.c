/*
** $Id: ltm.c,v 1.4 1997/10/24 17:17:24 roberto Exp roberto $
** Tag methods
** See Copyright Notice in lua.h
*/


#include <stdio.h>
#include <string.h>

#include "lauxlib.h"
#include "ldo.h"
#include "lmem.h"
#include "lobject.h"
#include "ltm.h"

static struct IM init_IM[NUM_TAGS] = {
{{{LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}},
  {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}},
  {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}},
  {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}},
  {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}},
  {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}}},
{{{LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}},
  {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}},
  {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}},
  {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}},
  {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}},
  {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}}},
{{{LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}},
  {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}},
  {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}},
  {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}},
  {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}},
  {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}}},
{{{LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}},
  {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}},
  {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}},
  {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}},
  {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}},
  {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}}},
{{{LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}},
  {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}},
  {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}},
  {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}},
  {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}},
  {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}}},
{{{LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}},
  {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}},
  {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}},
  {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}},
  {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}},
  {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}}},
{{{LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}},
  {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}},
  {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}},
  {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}},
  {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}},
  {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}, {LUA_T_NIL, {NULL}}}}
};




char *luaT_eventname[] = {  /* ORDER IM */
  "gettable", "settable", "index", "getglobal", "setglobal", "add",
  "sub", "mul", "div", "pow", "unm", "lt", "le", "gt", "ge",
  "concat", "gc", "function", NULL
};


static int luaI_checkevent (char *name, char *list[])
{
  int e = luaO_findstring(name, list);
  if (e < 0)
    luaL_verror("`%.50s' is not a valid event name", name);
  return e;
}


struct IM *luaT_IMtable = init_IM;

static int IMtable_size = NUM_TAGS;

static int last_tag = -(NUM_TAGS-1);


/* events in LUA_T_NIL are all allowed, since this is used as a
*  'placeholder' for "default" fallbacks
*/
static char validevents[NUM_TAGS][IM_N] = { /* ORDER LUA_T, ORDER IM */
{1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1},  /* LUA_T_USERDATA */
{1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1},  /* LUA_T_NUMBER */
{1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},  /* LUA_T_STRING */
{0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},  /* LUA_T_ARRAY */
{1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0},  /* LUA_T_PROTO */
{1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0},  /* LUA_T_CPROTO */
{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}   /* LUA_T_NIL */
};

static int validevent (lua_Type t, int e)
{ /* ORDER LUA_T */
  return (t < LUA_T_NIL) ?  1 : validevents[-t][e];
}


static void init_entry (int tag)
{
  int i;
  for (i=0; i<IM_N; i++)
    ttype(luaT_getim(tag, i)) = LUA_T_NIL;
}

int lua_newtag (void)
{
  --last_tag;
  if ((-last_tag) >= IMtable_size) {
    if (luaT_IMtable == init_IM) {  /* fist time? */
      IMtable_size *= 2;
      luaT_IMtable = luaM_newvector(IMtable_size, struct IM);
      memcpy(luaT_IMtable, init_IM, sizeof(init_IM));
    }
    else
      IMtable_size = luaM_growvector(&luaT_IMtable, IMtable_size,
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

void luaT_realtag (int tag)
{
  if (!(last_tag <= tag && tag < LUA_T_NIL))
    luaL_verror("tag %d is not result of `newtag'", tag);
}



int luaT_efectivetag (TObject *o)
{
  int t;
  switch (t = ttype(o)) {
    case LUA_T_USERDATA: {
      int tag = o->value.ts->u.d.tag;
      return (tag >= 0) ? LUA_T_USERDATA : tag;
    }
    case LUA_T_ARRAY:
      return o->value.a->htag;
    case LUA_T_FUNCTION:  case LUA_T_MARK:
      return o->value.cl->consts[0].ttype;
    default:
      return t;
  }
}


TObject *luaT_gettagmethod (int t, char *event)
{
  int e = luaI_checkevent(event, luaT_eventname);
  checktag(t);
  if (validevent(t, e))
    return luaT_getim(t,e);
  else
    return &luaO_nilobject;
}


void luaT_settagmethod (int t, char *event, TObject *func)
{
  TObject temp = *func;
  int e = luaI_checkevent(event, luaT_eventname);
  checktag(t);
  if (!validevent(t, e))
    luaL_verror("settagmethod: cannot change internal method `%s' for tag %d",
                luaT_eventname[e], t);
  *func = *luaT_getim(t,e);
  *luaT_getim(t, e) = temp;
}


char *luaT_travtagmethods (int (*fn)(TObject *))
{
  int e;
  if (fn(&luaD_errorim))
    return "error";
  for (e=IM_GETTABLE; e<=IM_FUNCTION; e++) {  /* ORDER IM */
    int t;
    for (t=0; t>=last_tag; t--)
      if (fn(luaT_getim(t,e)))
        return luaT_eventname[e];
  }
  return NULL;
}


/*
* ===================================================================
* compatibility with old fallback system
*/
#if	LUA_COMPAT2_5

#include "lapi.h"

static void errorFB (void)
{
  lua_Object o = lua_getparam(1);
  if (lua_isstring(o))
    fprintf(stderr, "lua: %s\n", lua_getstring(o));
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
      *luaT_getim(t, e) = *func;
}


void luaT_setfallback (void)
{
  static char *oldnames [] = {"error", "getglobal", "arith", "order", NULL};
  TObject oldfunc;
  lua_CFunction replace;
  char *name = luaL_check_string(1);
  lua_Object func = lua_getparam(2);
  luaL_arg_check(lua_isfunction(func), 2, "function expected");
  switch (luaO_findstring(name, oldnames)) {
    case 0:  /* old error fallback */
      oldfunc = luaD_errorim;
      luaD_errorim = *luaA_Address(func);
      replace = errorFB;
      break;
    case 1:  /* old getglobal fallback */
      oldfunc = *luaT_getim(LUA_T_NIL, IM_GETGLOBAL);
      *luaT_getim(LUA_T_NIL, IM_GETGLOBAL) = *luaA_Address(func);
      replace = nilFB;
      break;
    case 2: {  /* old arith fallback */
      int i;
      oldfunc = *luaT_getim(LUA_T_NUMBER, IM_POW);
      for (i=IM_ADD; i<=IM_UNM; i++)  /* ORDER IM */
        fillvalids(i, luaA_Address(func));
      replace = typeFB;
      break;
    }
    case 3: {  /* old order fallback */
      int i;
      oldfunc = *luaT_getim(LUA_T_NIL, IM_LT);
      for (i=IM_LT; i<=IM_GE; i++)  /* ORDER IM */
        fillvalids(i, luaA_Address(func));
      replace = typeFB;
      break;
    }
    default: {
      int e;
      if ((e = luaO_findstring(name, luaT_eventname)) >= 0) {
        oldfunc = *luaT_getim(LUA_T_NIL, e);
        fillvalids(e, luaA_Address(func));
        replace = (e == IM_GC || e == IM_INDEX) ? nilFB : typeFB;
      }
      else {
        luaL_verror("`%.50s' is not a valid fallback name", name);
        replace = NULL;  /* to avoid warnings */
      }
    }
  }
  if (oldfunc.ttype != LUA_T_NIL)
    luaA_pushobject(&oldfunc);
  else
    lua_pushcfunction(replace);
}
#endif

