/*
** $Id: ltm.c,v 1.62 2001/01/24 15:45:33 roberto Exp roberto $
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
#include "lstring.h"
#include "ltable.h"
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
static const unsigned char luaT_validevents[NUM_TAGS][TM_N] = {
  {1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1},  /* LUA_TUSERDATA */
  {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},  /* LUA_TNIL */
  {1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1},  /* LUA_TNUMBER */
  {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},  /* LUA_TSTRING */
  {0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1},  /* LUA_TTABLE */
  {1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0}   /* LUA_TFUNCTION */
};

int luaT_validevent (int t, int e) {  /* ORDER LUA_T */
  return (t >= NUM_TAGS) ?  1 : (int)luaT_validevents[t][e];
}


void luaT_init (lua_State *L) {
  static const char *const typenames[NUM_TAGS] = {
    "userdata", "nil", "number", "string", "table", "function"
  };
  int i;
  for (i=0; i<NUM_TAGS; i++)
    luaT_newtag(L, typenames[i], i);
}


int luaT_newtag (lua_State *L, const char *name, int basictype) {
  int tag;
  int i;
  TString *ts;
  luaM_growvector(L, G(L)->TMtable, G(L)->ntag, G(L)->sizeTM, struct TM,
                  MAX_INT, "tag table overflow");
  tag = G(L)->ntag;
  if (name == NULL)
    ts = NULL;
  else {
    TObject *v;
    ts = luaS_new(L, name);
    v = luaH_setstr(L, G(L)->type2tag, ts);
    if (ttype(v) != LUA_TNIL) return LUA_TNONE;  /* invalid name */
    setnvalue(v, tag);
  }
  for (i=0; i<TM_N; i++)
    luaT_gettm(G(L), tag, i) = NULL;
  G(L)->TMtable[tag].collected = NULL;
  G(L)->TMtable[tag].name = ts;
  G(L)->TMtable[tag].basictype = basictype;
  G(L)->ntag++;
  return tag;
}


static void checktag (lua_State *L, int tag) {
  if (!(0 <= tag && tag < G(L)->ntag))
    luaO_verror(L, "%d is not a valid tag", tag);
}


LUA_API int lua_copytagmethods (lua_State *L, int tagto, int tagfrom) {
  int e;
  LUA_ENTRY;
  checktag(L, tagto);
  checktag(L, tagfrom);
  for (e=0; e<TM_N; e++) {
    if (luaT_validevent(tagto, e))
      luaT_gettm(G(L), tagto, e) = luaT_gettm(G(L), tagfrom, e);
  }
  LUA_EXIT;
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


const char *luaT_typename (global_State *G, const TObject *o) {
  int t = ttype(o);
  int tag;
  TString *ts;
  switch (t) {
    case LUA_TUSERDATA:
      tag = tsvalue(o)->u.d.tag;
      break;
    case LUA_TTABLE:
      tag = hvalue(o)->htag;
      break;
    default:
      tag = t;
  }
  ts = G->TMtable[tag].name;
  if (ts == NULL)
    ts = G->TMtable[t].name;
  return ts->str;
}


LUA_API void lua_gettagmethod (lua_State *L, int t, const char *event) {
  int e;
  LUA_ENTRY;
  e = luaI_checkevent(L, event, t);
  checktag(L, t);
  if (luaT_validevent(t, e) && luaT_gettm(G(L), t, e)) {
    setclvalue(L->top, luaT_gettm(G(L), t, e));
  }
  else
    setnilvalue(L->top);
  incr_top;
  LUA_EXIT;
}


LUA_API void lua_settagmethod (lua_State *L, int t, const char *event) {
  int e;
  LUA_ENTRY;
  e = luaI_checkevent(L, event, t);
  checktag(L, t);
  if (!luaT_validevent(t, e))
    luaO_verror(L, "cannot change `%.20s' tag method for type `%.20s'%.20s",
                luaT_eventname[e], basictypename(G(L), t),
                (t == LUA_TTABLE || t == LUA_TUSERDATA) ?
                   " with default tag" : "");
  switch (ttype(L->top - 1)) {
    case LUA_TNIL:
      luaT_gettm(G(L), t, e) = NULL;
      break;
    case LUA_TFUNCTION:
      luaT_gettm(G(L), t, e) = clvalue(L->top - 1);
      break;
    default:
      luaD_error(L, "tag method must be a function (or nil)");
  }
  L->top--;
  LUA_EXIT;
}

