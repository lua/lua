/*
** $Id: ltm.c,v 1.56 2000/10/31 13:10:24 roberto Exp $
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


const char *const luaT_eventname[] = {  /* ORDER TM */
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
  if (e >= TM_N)
    luaO_verror(L, "event `%.50s' is deprecated", name);
  if (e == TM_GC && t == LUA_TTABLE)
    luaO_verror(L, "event `gc' for tables is deprecated");
  if (e < 0)
    luaO_verror(L, "`%.50s' is not a valid event name", name);
  return e;
}



/* events in LUA_TNIL are all allowed, since this is used as a
*  'placeholder' for "default" fallbacks
*/
/* ORDER LUA_T, ORDER TM */
static const char luaT_validevents[NUM_TAGS][TM_N] = {
  {1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1},  /* LUA_TUSERDATA */
  {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},  /* LUA_TNIL */
  {1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1},  /* LUA_TNUMBER */
  {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},  /* LUA_TSTRING */
  {0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1},  /* LUA_TTABLE */
  {1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0}   /* LUA_TFUNCTION */
};

int luaT_validevent (int t, int e) {  /* ORDER LUA_T */
  return (t >= NUM_TAGS) ?  1 : luaT_validevents[t][e];
}


static void init_entry (lua_State *L, int tag) {
  int i;
  for (i=0; i<TM_N; i++)
    luaT_gettm(L, tag, i) = NULL;
  L->TMtable[tag].collected = NULL;
}


void luaT_init (lua_State *L) {
  int t;
  luaM_growvector(L, L->TMtable, 0, NUM_TAGS, struct TM, "", MAX_INT);
  L->nblocks += NUM_TAGS*sizeof(struct TM);
  L->last_tag = NUM_TAGS-1;
  for (t=0; t<=L->last_tag; t++)
    init_entry(L, t);
}


LUA_API int lua_newtag (lua_State *L) {
  luaM_growvector(L, L->TMtable, L->last_tag, 1, struct TM,
                  "tag table overflow", MAX_INT);
  L->nblocks += sizeof(struct TM);
  L->last_tag++;
  init_entry(L, L->last_tag);
  return L->last_tag;
}


static void checktag (lua_State *L, int tag) {
  if (!(0 <= tag && tag <= L->last_tag))
    luaO_verror(L, "%d is not a valid tag", tag);
}

void luaT_realtag (lua_State *L, int tag) {
  if (!validtag(tag))
    luaO_verror(L, "tag %d was not created by `newtag'", tag);
}


LUA_API int lua_copytagmethods (lua_State *L, int tagto, int tagfrom) {
  int e;
  checktag(L, tagto);
  checktag(L, tagfrom);
  for (e=0; e<TM_N; e++) {
    if (luaT_validevent(tagto, e))
      luaT_gettm(L, tagto, e) = luaT_gettm(L, tagfrom, e);
  }
  return tagto;
}


int luaT_tag (const TObject *o) {
  int t = ttype(o);
  switch (t) {
    case LUA_TUSERDATA: return tsvalue(o)->u.d.tag;
    case LUA_TTABLE:    return hvalue(o)->htag;
    default:            return t;
  }
}


LUA_API void lua_gettagmethod (lua_State *L, int t, const char *event) {
  int e;
  e = luaI_checkevent(L, event, t);
  checktag(L, t);
  if (luaT_validevent(t, e) && luaT_gettm(L, t, e)) {
    clvalue(L->top) = luaT_gettm(L, t, e);
    ttype(L->top) = LUA_TFUNCTION;
  }
  else
    ttype(L->top) = LUA_TNIL;
  incr_top;
}


LUA_API void lua_settagmethod (lua_State *L, int t, const char *event) {
  int e = luaI_checkevent(L, event, t);
  checktag(L, t);
  if (!luaT_validevent(t, e))
    luaO_verror(L, "cannot change `%.20s' tag method for type `%.20s'%.20s",
                luaT_eventname[e], luaO_typenames[t],
                (t == LUA_TTABLE || t == LUA_TUSERDATA) ?
                   " with default tag" : "");
  switch (ttype(L->top - 1)) {
    case LUA_TNIL:
      luaT_gettm(L, t, e) = NULL;
      break;
    case LUA_TFUNCTION:
      luaT_gettm(L, t, e) = clvalue(L->top - 1);
      break;
    default:
      lua_error(L, "tag method must be a function (or nil)");
  }
  L->top--;
}

