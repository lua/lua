/*
** $Id: ltm.c,v 1.35 2000/03/20 19:14:54 roberto Exp roberto $
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
  "mul", "div", "pow", "unm", "lt", "concat", "gc", "function",
  "le", "gt", "ge",  /* deprecated options!! */
  NULL
};


static int luaI_checkevent (lua_State *L, const char *name, int t) {
  int e = luaL_findstring(name, luaT_eventname);
  if (e >= IM_N)
    luaL_verror(L, "event `%.50s' is deprecated", name);
  if (e == IM_GC && t == TAG_ARRAY)
    luaL_verror(L, "event `gc' for tables is deprecated");
  if (e < 0)
    luaL_verror(L, "`%.50s' is not a valid event name", name);
  return e;
}



/* events in TAG_NIL are all allowed, since this is used as a
*  'placeholder' for "default" fallbacks
*/
/* ORDER LUA_T, ORDER IM */
static const char luaT_validevents[NUM_TAGS][IM_N] = {
  {1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1},  /* TAG_USERDATA */
  {1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1},  /* TAG_NUMBER */
  {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},  /* TAG_STRING */
  {0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1},  /* TAG_ARRAY */
  {1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0},  /* TAG_LPROTO */
  {1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0},  /* TAG_CPROTO */
  {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}   /* TAG_NIL */
};

static int luaT_validevent (int t, int e) {  /* ORDER LUA_T */
  return (t > TAG_NIL) ?  1 : luaT_validevents[t][e];
}


static void init_entry (lua_State *L, int tag) {
  int i;
  for (i=0; i<IM_N; i++)
    ttype(luaT_getim(L, tag, i)) = TAG_NIL;
}


void luaT_init (lua_State *L) {
  int t;
  L->last_tag = NUM_TAGS-1;
  luaM_growvector(L, L->IMtable, 0, NUM_TAGS, struct IM, arrEM, MAX_INT);
  for (t=0; t<=L->last_tag; t++)
    init_entry(L, t);
}


int lua_newtag (lua_State *L) {
  ++L->last_tag;
  luaM_growvector(L, L->IMtable, L->last_tag, 1, struct IM, arrEM, MAX_INT);
  init_entry(L, L->last_tag);
  return L->last_tag;
}


static void checktag (lua_State *L, int tag) {
  if (!(0 <= tag && tag <= L->last_tag))
    luaL_verror(L, "%d is not a valid tag", tag);
}

void luaT_realtag (lua_State *L, int tag) {
  if (!(NUM_TAGS <= tag && tag <= L->last_tag))
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


int luaT_effectivetag (lua_State *L, const TObject *o) {
  static const int realtag[] = {  /* ORDER LUA_T */
    TAG_USERDATA, TAG_NUMBER, TAG_STRING, TAG_ARRAY,
    TAG_LPROTO, TAG_CPROTO, TAG_NIL,
    TAG_LPROTO, TAG_CPROTO,       /* TAG_LCLOSURE, TAG_CCLOSURE */
  };
  lua_Type t;
  switch (t = ttype(o)) {
    case TAG_USERDATA: {
      int tag = o->value.ts->u.d.tag;
      return (tag > L->last_tag) ? TAG_USERDATA : tag;  /* deprecated test */
    }
    case TAG_ARRAY:    return o->value.a->htag;
    default:             return realtag[t];
  }
}


const TObject *luaT_gettagmethod (lua_State *L, int t, const char *event) {
  int e;
  e = luaI_checkevent(L, event, t);
  checktag(L, t);
  if (luaT_validevent(t, e))
    return luaT_getim(L, t,e);
  else
    return &luaO_nilobject;
}


void luaT_settagmethod (lua_State *L, int t, const char *event, TObject *func) {
  TObject temp;
  int e;
  e = luaI_checkevent(L, event, t);
  checktag(L, t);
  if (!luaT_validevent(t, e))
    luaL_verror(L, "cannot change `%.20s' tag method for type `%.20s'%.20s",
                luaT_eventname[e], luaO_typenames[t],
                (t == TAG_ARRAY || t == TAG_USERDATA) ? " with default tag"
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
    for (t=0; t<=L->last_tag; t++)
      if (fn(L, luaT_getim(L, t,e)))
        return luaT_eventname[e];
  }
  return NULL;
}

