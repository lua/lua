/*
** $Id: ltm.c,v 1.31 2000/01/19 12:00:45 roberto Exp roberto $
** Tag methods
** See Copyright Notice in lua.h
*/


#include <stdio.h>
#include <string.h>

#define LUA_REENTRANT

#include "lauxlib.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "ltm.h"


const char *const luaT_eventname[] = {  /* ORDER IM */
  "gettable", "settable", "index", "getglobal", "setglobal", "add", "sub",
  "mul", "div", "pow", "unm", "lt", "concat", "gc", "function", NULL
};


static int luaI_checkevent (lua_State *L, const char *name, const char *const list[]) {
  int e = luaL_findstring(name, list);
  if (e < 0)
    luaL_verror(L, "`%.50s' is not a valid event name", name);
  return e;
}



/* events in LUA_T_NIL are all allowed, since this is used as a
*  'placeholder' for "default" fallbacks
*/
/* ORDER LUA_T, ORDER IM */
static const char luaT_validevents[NUM_TAGS][IM_N] = {
{1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1},  /* LUA_T_USERDATA */
{1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1},  /* LUA_T_NUMBER */
{1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},  /* LUA_T_STRING */
{0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1},  /* LUA_T_ARRAY */
{1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0},  /* LUA_T_LPROTO */
{1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0},  /* LUA_T_CPROTO */
{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}   /* LUA_T_NIL */
};

int luaT_validevent (int t, int e) {  /* ORDER LUA_T */
#ifdef LUA_COMPAT_GC
  if (t == LUA_T_ARRAY && e == IM_GC)
    return 1;  /* old versions allowed gc tag method for tables */
#endif
  return (t < LUA_T_NIL) ?  1 : luaT_validevents[-t][e];
}


static void init_entry (lua_State *L, int tag) {
  int i;
  for (i=0; i<IM_N; i++)
    ttype(luaT_getim(L, tag, i)) = LUA_T_NIL;
}


void luaT_init (lua_State *L) {
  int t;
  L->last_tag = -(NUM_TAGS-1);
  luaM_growvector(L, L->IMtable, 0, NUM_TAGS, struct IM, arrEM, MAX_INT);
  for (t=L->last_tag; t<=0; t++)
    init_entry(L, t);
}


int lua_newtag (lua_State *L) {
  --L->last_tag;
  luaM_growvector(L, L->IMtable, -(L->last_tag), 1, struct IM, arrEM, MAX_INT);
  init_entry(L, L->last_tag);
  return L->last_tag;
}


static void checktag (lua_State *L, int tag) {
  if (!(L->last_tag <= tag && tag <= 0))
    luaL_verror(L, "%d is not a valid tag", tag);
}

void luaT_realtag (lua_State *L, int tag) {
  if (!(L->last_tag <= tag && tag < LUA_T_NIL))
    luaL_verror(L, "tag %d was not created by `newtag'", tag);
}


int lua_copytagmethods (lua_State *L, int tagto, int tagfrom) {
  int e;
  checktag(L, tagto);
  checktag(L, tagfrom);
  for (e=0; e<IM_N; e++) {
    if (luaT_validevent(tagto, e))
      *luaT_getim(L, tagto, e) = *luaT_getim(L, tagfrom, e);
  }
  return tagto;
}


int luaT_effectivetag (const TObject *o) {
  static const int realtag[] = {  /* ORDER LUA_T */
    LUA_T_USERDATA, LUA_T_NUMBER, LUA_T_STRING, LUA_T_ARRAY,
    LUA_T_LPROTO, LUA_T_CPROTO, LUA_T_NIL,
    LUA_T_LPROTO, LUA_T_CPROTO,       /* LUA_T_LCLOSURE, LUA_T_CCLOSURE */
  };
  int t;
  switch (t = ttype(o)) {
    case LUA_T_USERDATA: {
      int tag = o->value.ts->u.d.tag;
      return (tag >= 0) ? LUA_T_USERDATA : tag;  /* deprecated test */
    }
    case LUA_T_ARRAY:    return o->value.a->htag;
    default:             return realtag[-t];
  }
}


const TObject *luaT_gettagmethod (lua_State *L, int t, const char *event) {
  int e;
#ifdef LUA_COMPAT_ORDER_TM
  static const char *old_order[] = {"le", "gt", "ge", NULL};
  if (luaL_findstring(event, old_order) >= 0)
     return &luaO_nilobject;
#endif
  e = luaI_checkevent(L, event, luaT_eventname);
  checktag(L, t);
  if (luaT_validevent(t, e))
    return luaT_getim(L, t,e);
  else
    return &luaO_nilobject;
}


void luaT_settagmethod (lua_State *L, int t, const char *event, TObject *func) {
  TObject temp;
  int e;
#ifdef LUA_COMPAT_ORDER_TM
  static const char *old_order[] = {"le", "gt", "ge", NULL};
  if (luaL_findstring(event, old_order) >= 0)
     return;  /* do nothing for old operators */
#endif
  e = luaI_checkevent(L, event, luaT_eventname);
  checktag(L, t);
  if (!luaT_validevent(t, e))
    luaL_verror(L, "cannot change tag method `%.20s' for type `%.20s'%.20s",
                luaT_eventname[e], luaO_typenames[-t],
                (t == LUA_T_ARRAY || t == LUA_T_USERDATA) ? " with default tag"
                                                          : "");
  temp = *func;
  *func = *luaT_getim(L, t,e);
  *luaT_getim(L, t, e) = temp;
}


const char *luaT_travtagmethods (lua_State *L,
                         int (*fn)(lua_State *, TObject *)) {  /* ORDER IM */
  int e;
  for (e=IM_GETTABLE; e<=IM_FUNCTION; e++) {
    int t;
    for (t=0; t>=L->last_tag; t--)
      if (fn(L, luaT_getim(L, t,e)))
        return luaT_eventname[e];
  }
  return NULL;
}

