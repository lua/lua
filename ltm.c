/*
** $Id: ltm.c,v 1.78 2001/08/31 19:46:07 roberto Exp $
** Tag methods
** See Copyright Notice in lua.h
*/


#include <stdio.h>
#include <string.h>

#define LUA_PRIVATE
#include "lua.h"

#include "ldo.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"


const l_char *const luaT_eventname[] = {  /* ORDER TM */
  l_s("gettable"), l_s("settable"), l_s("index"), l_s("getglobal"),
  l_s("setglobal"), l_s("add"), l_s("sub"), l_s("mul"), l_s("div"),
  l_s("pow"), l_s("unm"), l_s("lt"), l_s("concat"), l_s("gc"),
  l_s("function"),
  NULL
};


static int findevent (const l_char *name) {
  int i;
  for (i=0; luaT_eventname[i]; i++)
    if (strcmp(luaT_eventname[i], name) == 0)
      return i;
  return -1;  /* name not found */
}


static int luaI_checkevent (lua_State *L, const l_char *name) {
  int e = findevent(name);
  if (e < 0)
    luaO_verror(L, l_s("`%.50s' is not a valid event name"), name);
  return e;
}



/* events in LUA_TNIL are all allowed, since this is used as a
*  `placeholder' for default fallbacks
*/
/* ORDER LUA_T, ORDER TM */
static const lu_byte luaT_validevents[NUM_TAGS][TM_N] = {
  {1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1},  /* LUA_TUSERDATA */
  {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},  /* LUA_TNIL */
  {1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1},  /* LUA_TNUMBER */
  {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},  /* LUA_TSTRING */
  {0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1},  /* LUA_TTABLE */
  {1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0}   /* LUA_TFUNCTION */
};

static int luaT_validevent (int t, int e) {  /* ORDER LUA_T */
  return (t >= NUM_TAGS) ?  1 : cast(int, luaT_validevents[t][e]);
}


void luaT_init (lua_State *L) {
  static const l_char *const typenames[NUM_TAGS] = {
    l_s("userdata"), l_s("nil"), l_s("number"), l_s("string"),
    l_s("table"), l_s("function")
  };
  int i;
  for (i=0; i<NUM_TAGS; i++)
    luaT_newtag(L, typenames[i], i);
}


int luaT_newtag (lua_State *L, const l_char *name, int basictype) {
  int tag;
  int i;
  TString *ts = NULL;
  luaM_growvector(L, G(L)->TMtable, G(L)->ntag, G(L)->sizeTM, struct TM,
                  MAX_INT, l_s("tag table overflow"));
  tag = G(L)->ntag;
  if (name) {
    const TObject *v;
    TObject otag;
    ts = luaS_new(L, name);
    v = luaH_getstr(G(L)->type2tag, ts);
    if (ttype(v) == LUA_TNUMBER) return cast(int, nvalue(v));
    setnvalue(&otag, tag);
    luaH_setstr(L, G(L)->type2tag, ts, &otag);
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
    luaO_verror(L, l_s("%d is not a valid tag"), tag);
}


int luaT_tag (const TObject *o) {
  int t = ttype(o);
  switch (t) {
    case LUA_TUSERDATA: return uvalue(o)->uv.tag;
    case LUA_TTABLE:    return hvalue(o)->htag;
    default:            return t;
  }
}


const l_char *luaT_typename (global_State *G, const TObject *o) {
  int t = ttype(o);
  int tag;
  TString *ts;
  switch (t) {
    case LUA_TUSERDATA:
      tag = uvalue(o)->uv.tag;
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
  return getstr(ts);
}


LUA_API void lua_gettagmethod (lua_State *L, int t, const l_char *event) {
  int e;
  lua_lock(L);
  e = luaI_checkevent(L, event);
  checktag(L, t);
  if (luaT_validevent(t, e) && luaT_gettm(G(L), t, e)) {
    setclvalue(L->top, luaT_gettm(G(L), t, e));
  }
  else
    setnilvalue(L->top);
  incr_top;
  lua_unlock(L);
}


LUA_API void lua_settagmethod (lua_State *L, int t, const l_char *event) {
  int e;
  lua_lock(L);
  e = luaI_checkevent(L, event);
  checktag(L, t);
  if (!luaT_validevent(t, e))
    luaO_verror(L, l_s("cannot change `%.20s' tag method for type `%.20s'%.20s"),
                luaT_eventname[e], typenamebytag(G(L), t),
                (t == LUA_TTABLE || t == LUA_TUSERDATA) ?
                   l_s(" with default tag") : l_s(""));
  switch (ttype(L->top - 1)) {
    case LUA_TNIL:
      luaT_gettm(G(L), t, e) = NULL;
      break;
    case LUA_TFUNCTION:
      luaT_gettm(G(L), t, e) = clvalue(L->top - 1);
      break;
    default:
      luaD_error(L, l_s("tag method must be a function (or nil)"));
  }
  L->top--;
  lua_unlock(L);
}

