/*
** $Id: ltm.c,v 1.51 2000/10/02 20:10:55 roberto Exp roberto $
** Tag methods
** See Copyright Notice in lua.h
*/


#include <stdio.h>
#include <string.h>

#include "lua.h"

#include "ldo.h"
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


static int findevent (const char *name) {
  int i;
  for (i=0; luaT_eventname[i]; i++)
    if (strcmp(luaT_eventname[i], name) == 0)
      return i;
  return -1;  /* name not found */
}


static int luaI_checkevent (lua_State *L, const char *name, int t) {
  int e = findevent(name);
  if (e >= IM_N)
    luaO_verror(L, "event `%.50s' is deprecated", name);
  if (e == IM_GC && t == TAG_TABLE)
    luaO_verror(L, "event `gc' for tables is deprecated");
  if (e < 0)
    luaO_verror(L, "`%.50s' is not a valid event name", name);
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
  {0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1},  /* TAG_TABLE */
  {1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0},  /* TAG_LCLOSURE */
  {1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0},  /* TAG_CCLOSURE */
  {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}   /* TAG_NIL */
};

int luaT_validevent (int t, int e) {  /* ORDER LUA_T */
  return (t > TAG_NIL) ?  1 : luaT_validevents[t][e];
}


static void init_entry (lua_State *L, int tag) {
  int i;
  for (i=0; i<IM_N; i++)
    ttype(luaT_getim(L, tag, i)) = TAG_NIL;
  L->IMtable[tag].collected = NULL;
}


void luaT_init (lua_State *L) {
  int t;
  luaM_growvector(L, L->IMtable, 0, NUM_TAGS, struct IM, "", MAX_INT);
  L->nblocks += NUM_TAGS*sizeof(struct IM);
  L->last_tag = NUM_TAGS-1;
  for (t=0; t<=L->last_tag; t++)
    init_entry(L, t);
}


int lua_newtag (lua_State *L) {
  luaM_growvector(L, L->IMtable, L->last_tag, 1, struct IM,
                  "tag table overflow", MAX_INT);
  L->nblocks += sizeof(struct IM);
  L->last_tag++;
  init_entry(L, L->last_tag);
  return L->last_tag;
}


static void checktag (lua_State *L, int tag) {
  if (!(0 <= tag && tag <= L->last_tag))
    luaO_verror(L, "%d is not a valid tag", tag);
}

void luaT_realtag (lua_State *L, int tag) {
  if (!(NUM_TAGS <= tag && tag <= L->last_tag))
    luaO_verror(L, "tag %d was not created by `newtag'", tag);
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


const TObject *luaT_gettagmethods (lua_State *L, const TObject *o) {
  lua_Tag t = ttype(o);
  switch (t) {
    case TAG_USERDATA: {
      int tag = tsvalue(o)->u.d.tag;
      if (tag > L->last_tag)
        return L->IMtable[TAG_USERDATA].int_method;
      else
        return L->IMtable[tag].int_method;
    }
    case TAG_TABLE:
      return L->IMtable[hvalue(o)->htag].int_method;
    default:
      return L->IMtable[(int)t].int_method;;
  }
}


void lua_gettagmethod (lua_State *L, int t, const char *event) {
  int e;
  e = luaI_checkevent(L, event, t);
  checktag(L, t);
  if (luaT_validevent(t, e))
    *L->top = *luaT_getim(L, t,e);
  else
    ttype(L->top) = TAG_NIL;
  incr_top;
}


void lua_settagmethod (lua_State *L, int t, const char *event) {
  TObject temp;
  int e;
  LUA_ASSERT(lua_isnil(L, -1) || lua_isfunction(L, -1),
             "function or nil expected");
  e = luaI_checkevent(L, event, t);
  checktag(L, t);
  if (!luaT_validevent(t, e))
    luaO_verror(L, "cannot change `%.20s' tag method for type `%.20s'%.20s",
                luaT_eventname[e], lua_typename(L, luaO_tag2type(t)),
                (t == TAG_TABLE || t == TAG_USERDATA) ? " with default tag"
                                                          : "");
  temp = *(L->top - 1);
  *(L->top - 1) = *luaT_getim(L, t,e);
  *luaT_getim(L, t, e) = temp;
}

