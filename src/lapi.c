/*
** $Id: lapi.c,v 1.110 2000/10/30 12:50:09 roberto Exp $
** Lua API
** See Copyright Notice in lua.h
*/


#include <string.h>

#include "lua.h"

#include "lapi.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lvm.h"


const char lua_ident[] = "$Lua: " LUA_VERSION " " LUA_COPYRIGHT " $\n"
                               "$Authors: " LUA_AUTHORS " $";



#define Index(L,i)	((i) >= 0 ? (L->Cbase+((i)-1)) : (L->top+(i)))

#define api_incr_top(L)	incr_top




TObject *luaA_index (lua_State *L, int index) {
  return Index(L, index);
}


static TObject *luaA_indexAcceptable (lua_State *L, int index) {
  if (index >= 0) {
    TObject *o = L->Cbase+(index-1);
    if (o >= L->top) return NULL;
    else return o;
  }
  else return L->top+index;
}


void luaA_pushobject (lua_State *L, const TObject *o) {
  *L->top = *o;
  incr_top;
}

LUA_API int lua_stackspace (lua_State *L) {
  return (L->stack_last - L->top);
}



/*
** basic stack manipulation
*/


LUA_API int lua_gettop (lua_State *L) {
  return (L->top - L->Cbase);
}


LUA_API void lua_settop (lua_State *L, int index) {
  if (index >= 0)
    luaD_adjusttop(L, L->Cbase, index);
  else
    L->top = L->top+index+1;  /* index is negative */
}


LUA_API void lua_remove (lua_State *L, int index) {
  StkId p = luaA_index(L, index);
  while (++p < L->top) *(p-1) = *p;
  L->top--;
}


LUA_API void lua_insert (lua_State *L, int index) {
  StkId p = luaA_index(L, index);
  StkId q;
  for (q = L->top; q>p; q--)
    *q = *(q-1);
  *p = *L->top;
}


LUA_API void lua_pushvalue (lua_State *L, int index) {
  *L->top = *luaA_index(L, index);
  api_incr_top(L);
}



/*
** access functions (stack -> C)
*/


LUA_API int lua_type (lua_State *L, int index) {
  StkId o = luaA_indexAcceptable(L, index);
  return (o == NULL) ? LUA_TNONE : ttype(o);
}

LUA_API const char *lua_typename (lua_State *L, int t) {
  UNUSED(L);
  return (t == LUA_TNONE) ? "no value" : luaO_typenames[t];
}


LUA_API int lua_iscfunction (lua_State *L, int index) {
  StkId o = luaA_indexAcceptable(L, index);
  return (o == NULL) ? 0 : iscfunction(o);
}

LUA_API int lua_isnumber (lua_State *L, int index) {
  TObject *o = luaA_indexAcceptable(L, index);
  return (o == NULL) ? 0 : (tonumber(o) == 0);
}

LUA_API int lua_isstring (lua_State *L, int index) {
  int t = lua_type(L, index);
  return (t == LUA_TSTRING || t == LUA_TNUMBER);
}


LUA_API int lua_tag (lua_State *L, int index) {
  StkId o = luaA_indexAcceptable(L, index);
  return (o == NULL) ? LUA_NOTAG : luaT_tag(o);
}

LUA_API int lua_equal (lua_State *L, int index1, int index2) {
  StkId o1 = luaA_indexAcceptable(L, index1);
  StkId o2 = luaA_indexAcceptable(L, index2);
  if (o1 == NULL || o2 == NULL) return 0;  /* index out-of-range */
  else return luaO_equalObj(o1, o2);
}

LUA_API int lua_lessthan (lua_State *L, int index1, int index2) {
  StkId o1 = luaA_indexAcceptable(L, index1);
  StkId o2 = luaA_indexAcceptable(L, index2);
  if (o1 == NULL || o2 == NULL) return 0;  /* index out-of-range */
  else return luaV_lessthan(L, o1, o2, L->top);
}



LUA_API double lua_tonumber (lua_State *L, int index) {
  StkId o = luaA_indexAcceptable(L, index);
  return (o == NULL || tonumber(o)) ? 0 : nvalue(o);
}

LUA_API const char *lua_tostring (lua_State *L, int index) {
  StkId o = luaA_indexAcceptable(L, index);
  return (o == NULL || tostring(L, o)) ? NULL : svalue(o);
}

LUA_API size_t lua_strlen (lua_State *L, int index) {
  StkId o = luaA_indexAcceptable(L, index);
  return (o == NULL || tostring(L, o)) ? 0 : tsvalue(o)->len;
}

LUA_API lua_CFunction lua_tocfunction (lua_State *L, int index) {
  StkId o = luaA_indexAcceptable(L, index);
  return (o == NULL || !iscfunction(o)) ? NULL : clvalue(o)->f.c;
}

LUA_API void *lua_touserdata (lua_State *L, int index) {
  StkId o = luaA_indexAcceptable(L, index);
  return (o == NULL || ttype(o) != LUA_TUSERDATA) ? NULL :
                                                    tsvalue(o)->u.d.value;
}

LUA_API const void *lua_topointer (lua_State *L, int index) {
  StkId o = luaA_indexAcceptable(L, index);
  if (o == NULL) return NULL;
  switch (ttype(o)) {
    case LUA_TTABLE: 
      return hvalue(o);
    case LUA_TFUNCTION:
      return clvalue(o);
    default: return NULL;
  }
}



/*
** push functions (C -> stack)
*/


LUA_API void lua_pushnil (lua_State *L) {
  ttype(L->top) = LUA_TNIL;
  api_incr_top(L);
}


LUA_API void lua_pushnumber (lua_State *L, double n) {
  nvalue(L->top) = n;
  ttype(L->top) = LUA_TNUMBER;
  api_incr_top(L);
}


LUA_API void lua_pushlstring (lua_State *L, const char *s, size_t len) {
  tsvalue(L->top) = luaS_newlstr(L, s, len);
  ttype(L->top) = LUA_TSTRING;
  api_incr_top(L);
}


LUA_API void lua_pushstring (lua_State *L, const char *s) {
  if (s == NULL)
    lua_pushnil(L);
  else
    lua_pushlstring(L, s, strlen(s));
}


LUA_API void lua_pushcclosure (lua_State *L, lua_CFunction fn, int n) {
  luaV_Cclosure(L, fn, n);
}


LUA_API void lua_pushusertag (lua_State *L, void *u, int tag) {
  /* ORDER LUA_T */
  if (!(tag == LUA_ANYTAG || tag == LUA_TUSERDATA || validtag(tag)))
    luaO_verror(L, "invalid tag for a userdata (%d)", tag);
  tsvalue(L->top) = luaS_createudata(L, u, tag);
  ttype(L->top) = LUA_TUSERDATA;
  api_incr_top(L);
}



/*
** get functions (Lua -> stack)
*/


LUA_API void lua_getglobal (lua_State *L, const char *name) {
  StkId top = L->top;
  *top = *luaV_getglobal(L, luaS_new(L, name));
  L->top = top;
  api_incr_top(L);
}


LUA_API void lua_gettable (lua_State *L, int index) {
  StkId t = Index(L, index);
  StkId top = L->top;
  *(top-1) = *luaV_gettable(L, t);
  L->top = top;  /* tag method may change top */
}


LUA_API void lua_rawget (lua_State *L, int index) {
  StkId t = Index(L, index);
  LUA_ASSERT(ttype(t) == LUA_TTABLE, "table expected");
  *(L->top - 1) = *luaH_get(L, hvalue(t), L->top - 1);
}


LUA_API void lua_rawgeti (lua_State *L, int index, int n) {
  StkId o = Index(L, index);
  LUA_ASSERT(ttype(o) == LUA_TTABLE, "table expected");
  *L->top = *luaH_getnum(hvalue(o), n);
  api_incr_top(L);
}


LUA_API void lua_getglobals (lua_State *L) {
  hvalue(L->top) = L->gt;
  ttype(L->top) = LUA_TTABLE;
  api_incr_top(L);
}


LUA_API int lua_getref (lua_State *L, int ref) {
  if (ref == LUA_REFNIL)
    ttype(L->top) = LUA_TNIL;
  else if (0 <= ref && ref < L->refSize &&
          (L->refArray[ref].st == LOCK || L->refArray[ref].st == HOLD))
    *L->top = L->refArray[ref].o;
  else
    return 0;
  api_incr_top(L);
  return 1;
}


LUA_API void lua_newtable (lua_State *L) {
  hvalue(L->top) = luaH_new(L, 0);
  ttype(L->top) = LUA_TTABLE;
  api_incr_top(L);
}



/*
** set functions (stack -> Lua)
*/


LUA_API void lua_setglobal (lua_State *L, const char *name) {
  StkId top = L->top;
  luaV_setglobal(L, luaS_new(L, name));
  L->top = top-1;  /* remove element from the top */
}


LUA_API void lua_settable (lua_State *L, int index) {
  StkId t = Index(L, index);
  StkId top = L->top;
  luaV_settable(L, t, top-2);
  L->top = top-2;  /* pop index and value */
}


LUA_API void lua_rawset (lua_State *L, int index) {
  StkId t = Index(L, index);
  LUA_ASSERT(ttype(t) == LUA_TTABLE, "table expected");
  *luaH_set(L, hvalue(t), L->top-2) = *(L->top-1);
  L->top -= 2;
}


LUA_API void lua_rawseti (lua_State *L, int index, int n) {
  StkId o = Index(L, index);
  LUA_ASSERT(ttype(o) == LUA_TTABLE, "table expected");
  *luaH_setint(L, hvalue(o), n) = *(L->top-1);
  L->top--;
}


LUA_API void lua_setglobals (lua_State *L) {
  StkId newtable = --L->top;
  LUA_ASSERT(ttype(newtable) == LUA_TTABLE, "table expected");
  L->gt = hvalue(newtable);
}


LUA_API int lua_ref (lua_State *L,  int lock) {
  int ref;
  if (ttype(L->top-1) == LUA_TNIL)
    ref = LUA_REFNIL;
  else {
    if (L->refFree != NONEXT) {  /* is there a free place? */
      ref = L->refFree;
      L->refFree = L->refArray[ref].st;
    }
    else {  /* no more free places */
      luaM_growvector(L, L->refArray, L->refSize, 1, struct Ref,
                      "reference table overflow", MAX_INT);
      L->nblocks += sizeof(struct Ref);
      ref = L->refSize++;
    }
    L->refArray[ref].o = *(L->top-1);
    L->refArray[ref].st = lock ? LOCK : HOLD;
  }
  L->top--;
  return ref;
}


/*
** "do" functions (run Lua code)
** (most of them are in ldo.c)
*/

LUA_API void lua_rawcall (lua_State *L, int nargs, int nresults) {
  luaD_call(L, L->top-(nargs+1), nresults);
}


/*
** Garbage-collection functions
*/

/* GC values are expressed in Kbytes: #bytes/2^10 */
#define GCscale(x)		((int)((x)>>10))
#define GCunscale(x)		((unsigned long)(x)<<10)

LUA_API int lua_getgcthreshold (lua_State *L) {
  return GCscale(L->GCthreshold);
}

LUA_API int lua_getgccount (lua_State *L) {
  return GCscale(L->nblocks);
}

LUA_API void lua_setgcthreshold (lua_State *L, int newthreshold) {
  if (newthreshold > GCscale(ULONG_MAX))
    L->GCthreshold = ULONG_MAX;
  else
    L->GCthreshold = GCunscale(newthreshold);
  luaC_checkGC(L);
}


/*
** miscellaneous functions
*/

LUA_API void lua_settag (lua_State *L, int tag) {
  luaT_realtag(L, tag);
  switch (ttype(L->top-1)) {
    case LUA_TTABLE:
      hvalue(L->top-1)->htag = tag;
      break;
    case LUA_TUSERDATA:
      tsvalue(L->top-1)->u.d.tag = tag;
      break;
    default:
      luaO_verror(L, "cannot change the tag of a %.20s",
                  luaO_typename(L->top-1));
  }
}


LUA_API void lua_unref (lua_State *L, int ref) {
  if (ref >= 0) {
    LUA_ASSERT(ref < L->refSize && L->refArray[ref].st < 0, "invalid ref");
    L->refArray[ref].st = L->refFree;
    L->refFree = ref;
  }
}


LUA_API int lua_next (lua_State *L, int index) {
  StkId t = luaA_index(L, index);
  Node *n;
  LUA_ASSERT(ttype(t) == LUA_TTABLE, "table expected");
  n = luaH_next(L, hvalue(t), luaA_index(L, -1));
  if (n) {
    *(L->top-1) = *key(n);
    *L->top = *val(n);
    api_incr_top(L);
    return 1;
  }
  else {  /* no more elements */
    L->top -= 1;  /* remove key */
    return 0;
  }
}


LUA_API int lua_getn (lua_State *L, int index) {
  Hash *h = hvalue(luaA_index(L, index));
  const TObject *value = luaH_getstr(h, luaS_new(L, "n"));  /* value = h.n */
  if (ttype(value) == LUA_TNUMBER)
    return (int)nvalue(value);
  else {
    Number max = 0;
    int i = h->size;
    Node *n = h->node;
    while (i--) {
      if (ttype(key(n)) == LUA_TNUMBER &&
          ttype(val(n)) != LUA_TNIL &&
          nvalue(key(n)) > max)
        max = nvalue(key(n));
      n++;
    }
    return (int)max;
  }
}


LUA_API void lua_concat (lua_State *L, int n) {
  StkId top = L->top;
  luaV_strconc(L, n, top);
  L->top = top-(n-1);
  luaC_checkGC(L);
}


LUA_API void *lua_newuserdata (lua_State *L, size_t size) {
  TString *ts = luaS_newudata(L, size, NULL);
  tsvalue(L->top) = ts;
  ttype(L->top) = LUA_TUSERDATA;
  api_incr_top(L);
  return ts->u.d.value;
}

