/*
** $Id: lapi.c,v 1.131 2001/02/20 18:15:33 roberto Exp roberto $
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



#ifndef api_check
#define api_check(L, o)		/* nothing */
#endif

#define api_checknelems(L, n)	api_check(L, (n) <= (L->top - L->Cbase))

#define api_incr_top(L)	incr_top



TObject *luaA_index (lua_State *L, int index) {
  if (index > 0) {
    api_check(L, index <= L->top - L->Cbase);
    return L->Cbase+index-1;
  }
  else {
    api_check(L, index != 0 && -index <= L->top - L->Cbase);
    return L->top+index;
  }
}


static TObject *luaA_indexAcceptable (lua_State *L, int index) {
  if (index > 0) {
    TObject *o = L->Cbase+(index-1);
    api_check(L, index <= L->stack_last - L->Cbase);
    if (o >= L->top) return NULL;
    else return o;
  }
  else {
    api_check(L, index != 0 && -index <= L->top - L->Cbase);
    return L->top+index;
  }
}


void luaA_pushobject (lua_State *L, const TObject *o) {
  setobj(L->top, o);
  incr_top;
}

LUA_API int lua_stackspace (lua_State *L) {
  int i;
  LUA_LOCK(L);
  i = (L->stack_last - L->top);
  LUA_UNLOCK(L);
  return i;
}



/*
** basic stack manipulation
*/


LUA_API int lua_gettop (lua_State *L) {
  int i;
  LUA_LOCK(L);
  i = (L->top - L->Cbase);
  LUA_UNLOCK(L);
  return i;
}


LUA_API void lua_settop (lua_State *L, int index) {
  LUA_LOCK(L);
  if (index >= 0)
    luaD_adjusttop(L, L->Cbase, index);
  else {
    api_check(L, -(index+1) <= (L->top - L->Cbase));
    L->top = L->top+index+1;  /* index is negative */
  }
  LUA_UNLOCK(L);
}


LUA_API void lua_remove (lua_State *L, int index) {
  StkId p;
  LUA_LOCK(L);
  p = luaA_index(L, index);
  while (++p < L->top) setobj(p-1, p);
  L->top--;
  LUA_UNLOCK(L);
}


LUA_API void lua_insert (lua_State *L, int index) {
  StkId p;
  StkId q;
  LUA_LOCK(L);
  p = luaA_index(L, index);
  for (q = L->top; q>p; q--) setobj(q, q-1);
  setobj(p, L->top);
  LUA_UNLOCK(L);
}


LUA_API void lua_pushvalue (lua_State *L, int index) {
  LUA_LOCK(L);
  setobj(L->top, luaA_index(L, index));
  api_incr_top(L);
  LUA_UNLOCK(L);
}



/*
** access functions (stack -> C)
*/


LUA_API int lua_type (lua_State *L, int index) {
  StkId o;
  int i;
  LUA_LOCK(L);
  o = luaA_indexAcceptable(L, index);
  i = (o == NULL) ? LUA_TNONE : ttype(o);
  LUA_UNLOCK(L);
  return i;
}


LUA_API const char *lua_typename (lua_State *L, int t) {
  const char *s;
  LUA_LOCK(L);
  s = (t == LUA_TNONE) ? "no value" : basictypename(G(L), t);
  LUA_UNLOCK(L);
  return s;
}


LUA_API const char *lua_xtype (lua_State *L, int index) {
  StkId o;
  const char *type;
  LUA_LOCK(L);
  o = luaA_indexAcceptable(L, index);
  type = (o == NULL) ? "no value" : luaT_typename(G(L), o);
  LUA_UNLOCK(L);
  return type;
}


LUA_API int lua_iscfunction (lua_State *L, int index) {
  StkId o;
  int i;
  LUA_LOCK(L);
  o = luaA_indexAcceptable(L, index);
  i = (o == NULL) ? 0 : iscfunction(o);
  LUA_UNLOCK(L);
  return i;
}

LUA_API int lua_isnumber (lua_State *L, int index) {
  TObject *o;
  int i;
  LUA_LOCK(L);
  o = luaA_indexAcceptable(L, index);
  i = (o == NULL) ? 0 : (tonumber(o) == 0);
  LUA_UNLOCK(L);
  return i;
}

LUA_API int lua_isstring (lua_State *L, int index) {
  int t = lua_type(L, index);
  return (t == LUA_TSTRING || t == LUA_TNUMBER);
}


LUA_API int lua_tag (lua_State *L, int index) {
  StkId o;
  int i;
  LUA_LOCK(L);
  o = luaA_indexAcceptable(L, index);
  i = (o == NULL) ? LUA_NOTAG : luaT_tag(o);
  LUA_UNLOCK(L);
  return i;
}

LUA_API int lua_equal (lua_State *L, int index1, int index2) {
  StkId o1, o2;
  int i;
  LUA_LOCK(L);
  o1 = luaA_indexAcceptable(L, index1);
  o2 = luaA_indexAcceptable(L, index2);
  i = (o1 == NULL || o2 == NULL) ? 0  /* index out-of-range */
                                 : luaO_equalObj(o1, o2);
  LUA_UNLOCK(L);
  return i;
}

LUA_API int lua_lessthan (lua_State *L, int index1, int index2) {
  StkId o1, o2;
  int i;
  LUA_LOCK(L);
  o1 = luaA_indexAcceptable(L, index1);
  o2 = luaA_indexAcceptable(L, index2);
  i = (o1 == NULL || o2 == NULL) ? 0  /* index out-of-range */
                                 : luaV_lessthan(L, o1, o2);
  LUA_UNLOCK(L);
  return i;
}



LUA_API lua_Number lua_tonumber (lua_State *L, int index) {
  StkId o;
  lua_Number n;
  LUA_LOCK(L);
  o = luaA_indexAcceptable(L, index);
  n = (o == NULL || tonumber(o)) ? 0 : nvalue(o);
  LUA_UNLOCK(L);
  return n;
}

LUA_API const char *lua_tostring (lua_State *L, int index) {
  StkId o;
  const char *s;
  LUA_LOCK(L);
  o = luaA_indexAcceptable(L, index);
  s = (o == NULL || tostring(L, o)) ? NULL : svalue(o);
  LUA_UNLOCK(L);
  return s;
}

LUA_API size_t lua_strlen (lua_State *L, int index) {
  StkId o;
  size_t l;
  LUA_LOCK(L);
  o = luaA_indexAcceptable(L, index);
  l = (o == NULL || tostring(L, o)) ? 0 : tsvalue(o)->len;
  LUA_UNLOCK(L);
  return l;
}

LUA_API lua_CFunction lua_tocfunction (lua_State *L, int index) {
  StkId o;
  lua_CFunction f;
  LUA_LOCK(L);
  o = luaA_indexAcceptable(L, index);
  f = (o == NULL || !iscfunction(o)) ? NULL : clvalue(o)->f.c;
  LUA_UNLOCK(L);
  return f;
}

LUA_API void *lua_touserdata (lua_State *L, int index) {
  StkId o;
  void *p;
  LUA_LOCK(L);
  o = luaA_indexAcceptable(L, index);
  p = (o == NULL || ttype(o) != LUA_TUSERDATA) ? NULL :
                                                    tsvalue(o)->u.d.value;
  LUA_UNLOCK(L);
  return p;
}

LUA_API const void *lua_topointer (lua_State *L, int index) {
  StkId o;
  const void *p;
  LUA_LOCK(L);
  o = luaA_indexAcceptable(L, index);
  if (o == NULL) p = NULL;
  else {
    switch (ttype(o)) {
      case LUA_TTABLE: 
        p = hvalue(o);
        break;
      case LUA_TFUNCTION:
        p = clvalue(o);
        break;
      default:
        p = NULL;
        break;
    }
  }
  LUA_UNLOCK(L);
  return p;
}



/*
** push functions (C -> stack)
*/


LUA_API void lua_pushnil (lua_State *L) {
  LUA_LOCK(L);
  setnilvalue(L->top);
  api_incr_top(L);
  LUA_UNLOCK(L);
}


LUA_API void lua_pushnumber (lua_State *L, lua_Number n) {
  LUA_LOCK(L);
  setnvalue(L->top, n);
  api_incr_top(L);
  LUA_UNLOCK(L);
}


LUA_API void lua_pushlstring (lua_State *L, const char *s, size_t len) {
  LUA_LOCK(L);
  setsvalue(L->top, luaS_newlstr(L, s, len));
  api_incr_top(L);
  LUA_UNLOCK(L);
}


LUA_API void lua_pushstring (lua_State *L, const char *s) {
  if (s == NULL)
    lua_pushnil(L);
  else
    lua_pushlstring(L, s, strlen(s));
}


LUA_API void lua_pushcclosure (lua_State *L, lua_CFunction fn, int n) {
  LUA_LOCK(L);
  api_checknelems(L, n);
  luaV_Cclosure(L, fn, n);
  LUA_UNLOCK(L);
}


LUA_API int lua_pushuserdata (lua_State *L, void *u) {
  int isnew;
  LUA_LOCK(L);
  isnew = luaS_createudata(L, u, L->top);
  api_incr_top(L);
  LUA_UNLOCK(L);
  return isnew;
}



/*
** get functions (Lua -> stack)
*/


LUA_API void lua_getglobal (lua_State *L, const char *name) {
  LUA_LOCK(L);
  luaV_getglobal(L, luaS_new(L, name), L->top);
  api_incr_top(L);
  LUA_UNLOCK(L);
}


LUA_API void lua_gettable (lua_State *L, int index) {
  StkId t;
  LUA_LOCK(L);
  t = luaA_index(L, index);
  luaV_gettable(L, t, L->top-1, L->top-1);
  LUA_UNLOCK(L);
}


LUA_API void lua_rawget (lua_State *L, int index) {
  StkId t;
  LUA_LOCK(L);
  t = luaA_index(L, index);
  api_check(L, ttype(t) == LUA_TTABLE);
  setobj(L->top - 1, luaH_get(hvalue(t), L->top - 1));
  LUA_UNLOCK(L);
}


LUA_API void lua_rawgeti (lua_State *L, int index, int n) {
  StkId o;
  LUA_LOCK(L);
  o = luaA_index(L, index);
  api_check(L, ttype(o) == LUA_TTABLE);
  setobj(L->top, luaH_getnum(hvalue(o), n));
  api_incr_top(L);
  LUA_UNLOCK(L);
}


LUA_API void lua_getglobals (lua_State *L) {
  LUA_LOCK(L);
  sethvalue(L->top, L->gt);
  api_incr_top(L);
  LUA_UNLOCK(L);
}


LUA_API int lua_getref (lua_State *L, int ref) {
  int status = 1;
  LUA_LOCK(L);
  if (ref == LUA_REFNIL) {
    setnilvalue(L->top);
    api_incr_top(L);
  }
  else {
    api_check(L, 0 <= ref && ref < G(L)->nref);
    if (G(L)->refArray[ref].st != LOCK && G(L)->refArray[ref].st != HOLD)
      status = 0;
    else {
      setobj(L->top, &G(L)->refArray[ref].o);
      api_incr_top(L);
    }
  }
  LUA_UNLOCK(L);
  return status;
}


LUA_API void lua_newtable (lua_State *L) {
  LUA_LOCK(L);
  sethvalue(L->top, luaH_new(L, 0));
  api_incr_top(L);
  LUA_UNLOCK(L);
}



/*
** set functions (stack -> Lua)
*/


LUA_API void lua_setglobal (lua_State *L, const char *name) {
  LUA_LOCK(L);
  api_checknelems(L, 1);
  luaV_setglobal(L, luaS_new(L, name), L->top - 1);
  L->top--;  /* remove element from the top */
  LUA_UNLOCK(L);
}


LUA_API void lua_settable (lua_State *L, int index) {
  StkId t;
  LUA_LOCK(L);
  api_checknelems(L, 2);
  t = luaA_index(L, index);
  luaV_settable(L, t, L->top - 2, L->top - 1);
  L->top -= 2;  /* pop index and value */
  LUA_UNLOCK(L);
}


LUA_API void lua_rawset (lua_State *L, int index) {
  StkId t;
  LUA_LOCK(L);
  api_checknelems(L, 2);
  t = luaA_index(L, index);
  api_check(L, ttype(t) == LUA_TTABLE);
  setobj(luaH_set(L, hvalue(t), L->top-2), (L->top-1));
  L->top -= 2;
  LUA_UNLOCK(L);
}


LUA_API void lua_rawseti (lua_State *L, int index, int n) {
  StkId o;
  LUA_LOCK(L);
  api_checknelems(L, 1);
  o = luaA_index(L, index);
  api_check(L, ttype(o) == LUA_TTABLE);
  setobj(luaH_setnum(L, hvalue(o), n), (L->top-1));
  L->top--;
  LUA_UNLOCK(L);
}


LUA_API void lua_setglobals (lua_State *L) {
  StkId newtable;
  LUA_LOCK(L);
  api_checknelems(L, 1);
  newtable = --L->top;
  api_check(L, ttype(newtable) == LUA_TTABLE);
  L->gt = hvalue(newtable);
  LUA_UNLOCK(L);
}


LUA_API int lua_ref (lua_State *L,  int lock) {
  int ref;
  LUA_LOCK(L);
  api_checknelems(L, 1);
  if (ttype(L->top-1) == LUA_TNIL)
    ref = LUA_REFNIL;
  else {
    if (G(L)->refFree != NONEXT) {  /* is there a free place? */
      ref = G(L)->refFree;
      G(L)->refFree = G(L)->refArray[ref].st;
    }
    else {  /* no more free places */
      luaM_growvector(L, G(L)->refArray, G(L)->nref, G(L)->sizeref, struct Ref,
                      MAX_INT, "reference table overflow");
      ref = G(L)->nref++;
    }
    setobj(&G(L)->refArray[ref].o, L->top-1);
    G(L)->refArray[ref].st = lock ? LOCK : HOLD;
  }
  L->top--;
  LUA_UNLOCK(L);
  return ref;
}


/*
** `do' functions (run Lua code)
** (most of them are in ldo.c)
*/

LUA_API void lua_rawcall (lua_State *L, int nargs, int nresults) {
  LUA_LOCK(L);
  api_checknelems(L, nargs+1);
  luaD_call(L, L->top-(nargs+1), nresults);
  LUA_UNLOCK(L);
}


/*
** Garbage-collection functions
*/

/* GC values are expressed in Kbytes: #bytes/2^10 */
#define GCscale(x)		((int)((x)>>10))
#define GCunscale(x)		((lu_mem)(x)<<10)

LUA_API int lua_getgcthreshold (lua_State *L) {
  int threshold;
  LUA_LOCK(L);
  threshold = GCscale(G(L)->GCthreshold);
  LUA_UNLOCK(L);
  return threshold;
}

LUA_API int lua_getgccount (lua_State *L) {
  int count;
  LUA_LOCK(L);
  count = GCscale(G(L)->nblocks);
  LUA_UNLOCK(L);
  return count;
}

LUA_API void lua_setgcthreshold (lua_State *L, int newthreshold) {
  LUA_LOCK(L);
  if (newthreshold > GCscale(ULONG_MAX))
    G(L)->GCthreshold = ULONG_MAX;
  else
    G(L)->GCthreshold = GCunscale(newthreshold);
  luaC_checkGC(L);
  LUA_UNLOCK(L);
}


/*
** miscellaneous functions
*/

LUA_API int lua_newtype (lua_State *L, const char *name, int basictype) {
  int tag;
  LUA_LOCK(L);
  if (basictype != LUA_TNONE &&
      basictype != LUA_TTABLE &&
      basictype != LUA_TUSERDATA)
    luaO_verror(L, "invalid basic type (%d) for new type", basictype);
  tag = luaT_newtag(L, name, basictype);
  if (tag == LUA_TNONE)
    luaO_verror(L, "type name '%.30s' already exists", name);
  LUA_UNLOCK(L);
  return tag;
}


LUA_API int lua_type2tag (lua_State *L, const char *name) {
  int tag;
  const TObject *v;
  LUA_LOCK(L);
  v = luaH_getstr(G(L)->type2tag, luaS_new(L, name));
  if (ttype(v) == LUA_TNIL)
    tag = LUA_TNONE;
  else {
    lua_assert(ttype(v) == LUA_TNUMBER);
    tag = (int)nvalue(v);
  }
  LUA_UNLOCK(L);
  return tag;
}


LUA_API void lua_settag (lua_State *L, int tag) {
  int basictype;
  LUA_LOCK(L);
  api_checknelems(L, 1);
  if (tag < 0 || tag >= G(L)->ntag)
    luaO_verror(L, "%d is not a valid tag", tag);
  basictype = G(L)->TMtable[tag].basictype;
  if (basictype != LUA_TNONE && basictype != ttype(L->top-1))
    luaO_verror(L, "tag %d can only be used for type '%.20s'", tag,
                basictypename(G(L), basictype));
  switch (ttype(L->top-1)) {
    case LUA_TTABLE:
      hvalue(L->top-1)->htag = tag;
      break;
    case LUA_TUSERDATA:
      tsvalue(L->top-1)->u.d.tag = tag;
      break;
    default:
      luaO_verror(L, "cannot change the tag of a %.20s",
                  luaT_typename(G(L), L->top-1));
  }
  LUA_UNLOCK(L);
}


LUA_API void lua_error (lua_State *L, const char *s) {
  LUA_LOCK(L);
  luaD_error(L, s);
  LUA_UNLOCK(L);
}


LUA_API void lua_unref (lua_State *L, int ref) {
  LUA_LOCK(L);
  if (ref >= 0) {
    api_check(L, ref < G(L)->nref && G(L)->refArray[ref].st < 0);
    G(L)->refArray[ref].st = G(L)->refFree;
    G(L)->refFree = ref;
  }
  LUA_UNLOCK(L);
}


LUA_API int lua_next (lua_State *L, int index) {
  StkId t;
  Node *n;
  int more;
  LUA_LOCK(L);
  t = luaA_index(L, index);
  api_check(L, ttype(t) == LUA_TTABLE);
  n = luaH_next(L, hvalue(t), luaA_index(L, -1));
  if (n) {
    setkey2obj(L->top-1, n);
    setobj(L->top, val(n));
    api_incr_top(L);
    more = 1;
  }
  else {  /* no more elements */
    L->top -= 1;  /* remove key */
    more = 0;
  }
  LUA_UNLOCK(L);
  return more;
}


LUA_API int lua_getn (lua_State *L, int index) {
  Hash *h;
  const TObject *value;
  int n;
  LUA_LOCK(L);
  h = hvalue(luaA_index(L, index));
  value = luaH_getstr(h, luaS_newliteral(L, "n"));  /* = h.n */
  if (ttype(value) == LUA_TNUMBER)
    n = (int)nvalue(value);
  else {
    lua_Number max = 0;
    int i = h->size;
    Node *nd = h->node;
    while (i--) {
      if (ttype_key(nd) == LUA_TNUMBER &&
          ttype(val(nd)) != LUA_TNIL &&
          nvalue_key(nd) > max)
        max = nvalue_key(nd);
      nd++;
    }
    n = (int)max;
  }
  LUA_UNLOCK(L);
  return n;
}


LUA_API void lua_concat (lua_State *L, int n) {
  LUA_LOCK(L);
  api_checknelems(L, n);
  if (n >= 2) {
    luaV_strconc(L, n, L->top);
    L->top -= (n-1);
    luaC_checkGC(L);
  }
  else if (n == 0) {  /* push null string */
    setsvalue(L->top, luaS_newlstr(L, NULL, 0));
    api_incr_top(L);
  }
  /* else n == 1; nothing to do */
  LUA_UNLOCK(L);
}


LUA_API void *lua_newuserdata (lua_State *L, size_t size) {
  TString *ts;
  void *p;
  LUA_LOCK(L);
  if (size == 0) size = 1;
  ts = luaS_newudata(L, size, NULL);
  setuvalue(L->top, ts);
  api_incr_top(L);
  p = ts->u.d.value;
  LUA_UNLOCK(L);
  return p;
}

