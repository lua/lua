/*
** $Id: ltm.c,v 1.27 1999/09/20 14:57:29 roberto Exp roberto $
** Tag methods
** See Copyright Notice in lua.h
*/


#include <stdio.h>
#include <string.h>

#include "lauxlib.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "ltm.h"


const char *const luaT_eventname[] = {  /* ORDER IM */
  "gettable", "settable", "index", "getglobal", "setglobal", "add",
  "sub", "mul", "div", "pow", "unm", "lt", "le", "gt", "ge",
  "concat", "gc", "function", NULL
};


static int luaI_checkevent (const char *name, const char *const list[]) {
  int e = luaL_findstring(name, list);
  if (e < 0)
    luaL_verror("`%.50s' is not a valid event name", name);
  return e;
}



/* events in LUA_T_NIL are all allowed, since this is used as a
*  'placeholder' for "default" fallbacks
*/
/* ORDER LUA_T, ORDER IM */
static const char luaT_validevents[NUM_TAGS][IM_N] = {
{1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1},  /* LUA_T_USERDATA */
{1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1},  /* LUA_T_NUMBER */
{1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},  /* LUA_T_STRING */
{0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1},  /* LUA_T_ARRAY */
{1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0},  /* LUA_T_PROTO */
{1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0},  /* LUA_T_CPROTO */
{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}   /* LUA_T_NIL */
};

int luaT_validevent (int t, int e) {  /* ORDER LUA_T */
#ifdef LUA_COMPAT_GC
  if (t == LUA_T_ARRAY && e == IM_GC)
    return 1;  /* old versions allowed gc tag method for tables */
#endif
  return (t < LUA_T_NIL) ?  1 : luaT_validevents[-t][e];
}


static void init_entry (int tag) {
  int i;
  for (i=0; i<IM_N; i++)
    ttype(luaT_getim(tag, i)) = LUA_T_NIL;
}


void luaT_init (void) {
  int t;
  L->last_tag = -(NUM_TAGS-1);
  luaM_growvector(L->IMtable, 0, NUM_TAGS, struct IM, arrEM, MAX_INT);
  for (t=L->last_tag; t<=0; t++)
    init_entry(t);
}


int lua_newtag (void) {
  --L->last_tag;
  luaM_growvector(L->IMtable, -(L->last_tag), 1, struct IM, arrEM, MAX_INT);
  init_entry(L->last_tag);
  return L->last_tag;
}


static void checktag (int tag) {
  if (!(L->last_tag <= tag && tag <= 0))
    luaL_verror("%d is not a valid tag", tag);
}

void luaT_realtag (int tag) {
  if (!(L->last_tag <= tag && tag < LUA_T_NIL))
    luaL_verror("tag %d was not created by `newtag'", tag);
}


int lua_copytagmethods (int tagto, int tagfrom) {
  int e;
  checktag(tagto);
  checktag(tagfrom);
  for (e=0; e<IM_N; e++) {
    if (luaT_validevent(tagto, e))
      *luaT_getim(tagto, e) = *luaT_getim(tagfrom, e);
  }
  return tagto;
}


int luaT_effectivetag (const TObject *o) {
  int t;
  switch (t = ttype(o)) {
    case LUA_T_ARRAY:
      return o->value.a->htag;
    case LUA_T_USERDATA: {
      int tag = o->value.ts->u.d.tag;
      return (tag >= 0) ? LUA_T_USERDATA : tag;
    }
    case LUA_T_CLOSURE:
      return o->value.cl->consts[0].ttype;
#ifdef DEBUG
    case LUA_T_PMARK: case LUA_T_CMARK:
    case LUA_T_CLMARK: case LUA_T_LINE:
      LUA_INTERNALERROR("invalid type");
#endif
    default:
      return t;
  }
}


const TObject *luaT_gettagmethod (int t, const char *event) {
  int e = luaI_checkevent(event, luaT_eventname);
  checktag(t);
  if (luaT_validevent(t, e))
    return luaT_getim(t,e);
  else
    return &luaO_nilobject;
}


void luaT_settagmethod (int t, const char *event, TObject *func) {
  TObject temp;
  int e = luaI_checkevent(event, luaT_eventname);
  checktag(t);
  if (!luaT_validevent(t, e))
    luaL_verror("cannot change tag method `%.20s' for type `%.20s'%.20s",
                luaT_eventname[e], luaO_typenames[-t],
                (t == LUA_T_ARRAY || t == LUA_T_USERDATA) ? " with default tag"
                                                          : "");
  temp = *func;
  *func = *luaT_getim(t,e);
  *luaT_getim(t, e) = temp;
}


const char *luaT_travtagmethods (int (*fn)(TObject *)) {  /* ORDER IM */
  int e;
  for (e=IM_GETTABLE; e<=IM_FUNCTION; e++) {
    int t;
    for (t=0; t>=L->last_tag; t--)
      if (fn(luaT_getim(t,e)))
        return luaT_eventname[e];
  }
  return NULL;
}

