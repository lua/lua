/*
** $Id: lapi.c,v 1.1 2001/11/29 22:14:34 rieru Exp rieru $
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


const char lua_ident[] =
  "$Lua: " LUA_VERSION " " LUA_COPYRIGHT " $\n"
  "$Authors: " LUA_AUTHORS " $\n"
  "$URL: www.lua.org $\n";



#ifndef api_check
#define api_check(L, o)		/* nothing */
#endif

#define api_checknelems(L, n)	api_check(L, (n) <= (L->top - L->ci->base))

#define api_incr_top(L)	incr_top



static TObject *negindex (lua_State *L, int index) {
  if (index > LUA_REGISTRYINDEX) {
    api_check(L, index != 0 && -index <= L->top - L->ci->base);
    return L->top+index;
  }
  else switch (index) {  /* pseudo-indices */
    case LUA_REGISTRYINDEX: return registry(L);
    case LUA_GLOBALSINDEX: return gt(L);
    default: {
      TObject *func = (L->ci->base - 1);
      index = LUA_GLOBALSINDEX - index;
      api_check(L, iscfunction(func) && index <= clvalue(func)->c.nupvalues);
      return &clvalue(func)->c.upvalue[index-1];
    }
  }
}


TObject *luaA_index (lua_State *L, int index) {
  if (index > 0) {
    api_check(L, index <= L->top - L->ci->base);
    return L->ci->base+index-1;
  }
  else
    return negindex(L, index);
}


static TObject *luaA_indexAcceptable (lua_State *L, int index) {
  if (index > 0) {
    TObject *o = L->ci->base+(index-1);
    api_check(L, index <= L->stack_last - L->ci->base);
    if (o >= L->top) return NULL;
    else return o;
  }
  else
    return negindex(L, index);
}


void luaA_pushobject (lua_State *L, const TObject *o) {
  setobj(L->top, o);
  incr_top;
}

LUA_API int lua_stackspace (lua_State *L) {
  return (L->stack_last - L->top);
}



/*
** basic stack manipulation
*/


LUA_API int lua_gettop (lua_State *L) {
  return (L->top - L->ci->base);
}


LUA_API void lua_settop (lua_State *L, int index) {
  lua_lock(L);
  if (index >= 0) {
    api_check(L, index <= L->stack_last - L->ci->base);
    luaD_adjusttop(L, L->ci->base+index);
  }
  else {
    api_check(L, -(index+1) <= (L->top - L->ci->base));
    L->top += index+1;  /* `subtract' index (index is negative) */
  }
  lua_unlock(L);
}


LUA_API void lua_remove (lua_State *L, int index) {
  StkId p;
  lua_lock(L);
  p = luaA_index(L, index);
  while (++p < L->top) setobj(p-1, p);
  L->top--;
  lua_unlock(L);
}


LUA_API void lua_insert (lua_State *L, int index) {
  StkId p;
  StkId q;
  lua_lock(L);
  p = luaA_index(L, index);
  for (q = L->top; q>p; q--) setobj(q, q-1);
  setobj(p, L->top);
  lua_unlock(L);
}


LUA_API void lua_pushvalue (lua_State *L, int index) {
  lua_lock(L);
  setobj(L->top, luaA_index(L, index));
  api_incr_top(L);
  lua_unlock(L);
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
  return (t == LUA_TNONE) ? "no value" : luaT_typenames[t];
}


LUA_API int lua_iscfunction (lua_State *L, int index) {
  StkId o = luaA_indexAcceptable(L, index);
  return (o == NULL) ? 0 : iscfunction(o);
}


LUA_API int lua_isnumber (lua_State *L, int index) {
  TObject n;
  TObject *o = luaA_indexAcceptable(L, index);
  return (o != NULL && (ttype(o) == LUA_TNUMBER || luaV_tonumber(o, &n)));
}


LUA_API int lua_istrue (lua_State *L, int index) {
  TObject *o = luaA_indexAcceptable(L, index);
  return (o != NULL && !l_isfalse(o));
}


LUA_API int lua_isstring (lua_State *L, int index) {
  int t = lua_type(L, index);
  return (t == LUA_TSTRING || t == LUA_TNUMBER);
}


LUA_API int lua_equal (lua_State *L, int index1, int index2) {
  StkId o1 = luaA_indexAcceptable(L, index1);
  StkId o2 = luaA_indexAcceptable(L, index2);
  return (o1 == NULL || o2 == NULL) ? 0  /* index out of range */
                                 : luaO_equalObj(o1, o2);
}


LUA_API int lua_lessthan (lua_State *L, int index1, int index2) {
  StkId o1, o2;
  int i;
  lua_lock(L);  /* may call tag method */
  o1 = luaA_indexAcceptable(L, index1);
  o2 = luaA_indexAcceptable(L, index2);
  i = (o1 == NULL || o2 == NULL) ? 0  /* index out-of-range */
                                 : luaV_lessthan(L, o1, o2);
  lua_unlock(L);
  return i;
}



LUA_API lua_Number lua_tonumber (lua_State *L, int index) {
  TObject n;
  const TObject *o = luaA_indexAcceptable(L, index);
  if (o != NULL &&
      (ttype(o) == LUA_TNUMBER || (o = luaV_tonumber(o, &n)) != NULL))
    return nvalue(o);
  else
    return 0;
}


LUA_API int lua_toboolean (lua_State *L, int index) {
  const TObject *o = luaA_indexAcceptable(L, index);
  if (o != NULL && (ttype(o) == LUA_TBOOLEAN))
    return bvalue(o);
  else
    return -1;
}


LUA_API const char *lua_tostring (lua_State *L, int index) {
  StkId o = luaA_indexAcceptable(L, index);
  if (o == NULL)
    return NULL;
  else if (ttype(o) == LUA_TSTRING)
    return svalue(o);
  else {
    const char *s;
    lua_lock(L);  /* `luaV_tostring' may create a new string */
    s = (luaV_tostring(L, o) == 0) ? svalue(o) : NULL;
    lua_unlock(L);
    return s;
  }
}


LUA_API size_t lua_strlen (lua_State *L, int index) {
  StkId o = luaA_indexAcceptable(L, index);
  if (o == NULL)
    return 0;
  else if (ttype(o) == LUA_TSTRING)
    return tsvalue(o)->tsv.len;
  else {
    size_t l;
    lua_lock(L);  /* `luaV_tostring' may create a new string */
    l = (luaV_tostring(L, o) == 0) ? tsvalue(o)->tsv.len : 0;
    lua_unlock(L);
    return l;
  }
}


LUA_API lua_CFunction lua_tocfunction (lua_State *L, int index) {
  StkId o = luaA_indexAcceptable(L, index);
  return (o == NULL || !iscfunction(o)) ? NULL : clvalue(o)->c.f;
}


LUA_API void *lua_touserdata (lua_State *L, int index) {
  StkId o = luaA_indexAcceptable(L, index);
  return (o == NULL || ttype(o) != LUA_TUSERDATA) ? NULL : uvalue(o)->uv.value;
}


LUA_API const void *lua_topointer (lua_State *L, int index) {
  StkId o = luaA_indexAcceptable(L, index);
  if (o == NULL) return NULL;
  else {
    switch (ttype(o)) {
      case LUA_TTABLE: return hvalue(o);
      case LUA_TFUNCTION: return clvalue(o);
      default: return NULL;
    }
  }
}



/*
** push functions (C -> stack)
*/


LUA_API void lua_pushnil (lua_State *L) {
  lua_lock(L);
  setnilvalue(L->top);
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API void lua_pushnumber (lua_State *L, lua_Number n) {
  lua_lock(L);
  setnvalue(L->top, n);
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API void lua_pushlstring (lua_State *L, const char *s, size_t len) {
  lua_lock(L);
  setsvalue(L->top, luaS_newlstr(L, s, len));
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API void lua_pushstring (lua_State *L, const char *s) {
  if (s == NULL)
    lua_pushnil(L);
  else
    lua_pushlstring(L, s, strlen(s));
}


LUA_API void lua_pushcclosure (lua_State *L, lua_CFunction fn, int n) {
  Closure *cl;
  lua_lock(L);
  api_checknelems(L, n);
  cl = luaF_newCclosure(L, n);
  cl->c.f = fn;
  L->top -= n;
  while (n--)
    setobj(&cl->c.upvalue[n], L->top+n);
  setclvalue(L->top, cl);
  incr_top;
  lua_unlock(L);
}


LUA_API void lua_pushboolean (lua_State *L, int b) {
  lua_lock(L);
  setbvalue(L->top, b);
  api_incr_top(L);
  lua_unlock(L);
}



/*
** get functions (Lua -> stack)
*/


LUA_API void lua_getstr (lua_State *L, int index, const char *name) {
  TObject o;
  lua_lock(L);
  setsvalue(&o, luaS_new(L, name));
  luaV_gettable(L, luaA_index(L, index), &o, L->top);
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API void lua_gettable (lua_State *L, int index) {
  StkId t;
  lua_lock(L);
  t = luaA_index(L, index);
  luaV_gettable(L, t, L->top-1, L->top-1);
  lua_unlock(L);
}


LUA_API void lua_rawget (lua_State *L, int index) {
  StkId t;
  lua_lock(L);
  t = luaA_index(L, index);
  api_check(L, ttype(t) == LUA_TTABLE);
  setobj(L->top - 1, luaH_get(hvalue(t), L->top - 1));
  lua_unlock(L);
}


LUA_API void lua_rawgeti (lua_State *L, int index, int n) {
  StkId o;
  lua_lock(L);
  o = luaA_index(L, index);
  api_check(L, ttype(o) == LUA_TTABLE);
  setobj(L->top, luaH_getnum(hvalue(o), n));
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API void lua_newtable (lua_State *L) {
  lua_lock(L);
  sethvalue(L->top, luaH_new(L, 0, 0));
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API void lua_geteventtable (lua_State *L, int objindex) {
  StkId obj;
  Table *et;
  lua_lock(L);
  obj = luaA_indexAcceptable(L, objindex);
  switch (ttype(obj)) {
    case LUA_TTABLE:
      et = hvalue(obj)->eventtable;
      break;
    case LUA_TUSERDATA:
      et = uvalue(obj)->uv.eventtable;
      break;
    default:
      et = hvalue(defaultet(L));
  }
  if (et == hvalue(defaultet(L)))
    setnilvalue(L->top);
  else
    sethvalue(L->top, et);
  api_incr_top(L);
  lua_unlock(L);
}


/*
** set functions (stack -> Lua)
*/


LUA_API void lua_setstr (lua_State *L, int index, const char *name) {
  TObject o;
  lua_lock(L);
  api_checknelems(L, 1);
  setsvalue(&o, luaS_new(L, name));
  luaV_settable(L, luaA_index(L, index), &o, L->top - 1);
  L->top--;  /* remove element from the top */
  lua_unlock(L);
}


LUA_API void lua_settable (lua_State *L, int index) {
  StkId t;
  lua_lock(L);
  api_checknelems(L, 2);
  t = luaA_index(L, index);
  luaV_settable(L, t, L->top - 2, L->top - 1);
  L->top -= 2;  /* pop index and value */
  lua_unlock(L);
}


LUA_API void lua_rawset (lua_State *L, int index) {
  StkId t;
  lua_lock(L);
  api_checknelems(L, 2);
  t = luaA_index(L, index);
  api_check(L, ttype(t) == LUA_TTABLE);
  luaH_set(L, hvalue(t), L->top-2, L->top-1);
  L->top -= 2;
  lua_unlock(L);
}


LUA_API void lua_rawseti (lua_State *L, int index, int n) {
  StkId o;
  lua_lock(L);
  api_checknelems(L, 1);
  o = luaA_index(L, index);
  api_check(L, ttype(o) == LUA_TTABLE);
  luaH_setnum(L, hvalue(o), n, L->top-1);
  L->top--;
  lua_unlock(L);
}


LUA_API void lua_setglobals (lua_State *L) {
  StkId newtable;
  lua_lock(L);
  api_checknelems(L, 1);
  newtable = --L->top;
  api_check(L, ttype(newtable) == LUA_TTABLE);
  setobj(gt(L), newtable);
  lua_unlock(L);
}


LUA_API void lua_seteventtable (lua_State *L, int objindex) {
  StkId obj, et;
  lua_lock(L);
  api_checknelems(L, 1);
  obj = luaA_indexAcceptable(L, objindex);
  et = --L->top;
  api_check(L, ttype(et) == LUA_TTABLE);
  switch (ttype(obj)) {
    case LUA_TTABLE:
      hvalue(obj)->eventtable = hvalue(et);
      break;
    case LUA_TUSERDATA:
      uvalue(obj)->uv.eventtable = hvalue(et);
      break;
    default:
      luaO_verror(L, "cannot change the event table of a %.20s",
                  luaT_typenames[ttype(obj)]);
  }
  lua_unlock(L);
}


/*
** `do' functions (run Lua code)
*/

LUA_API void lua_rawcall (lua_State *L, int nargs, int nresults) {
  StkId func;
  lua_lock(L);
  api_checknelems(L, nargs+1);
  func = L->top - (nargs+1);
  luaD_call(L, func);
  if (nresults != LUA_MULTRET)
    luaD_adjusttop(L, func + nresults);
  lua_unlock(L);
}


LUA_API int lua_dofile (lua_State *L, const char *filename) {
  int status;
  status = lua_loadfile(L, filename);
  if (status == 0)  /* parse OK? */
    status = lua_call(L, 0, LUA_MULTRET);  /* call main */
  return status;
}


LUA_API int lua_dobuffer (lua_State *L, const char *buff, size_t size,
                          const char *name) {
  int status;
  status = lua_loadbuffer(L, buff, size, name);
  if (status == 0)  /* parse OK? */
    status = lua_call(L, 0, LUA_MULTRET);  /* call main */
  return status;
}


LUA_API int lua_dostring (lua_State *L, const char *str) {
  return lua_dobuffer(L, str, strlen(str), str);
}



/*
** Garbage-collection functions
*/

/* GC values are expressed in Kbytes: #bytes/2^10 */
#define GCscale(x)		(cast(int, (x)>>10))
#define GCunscale(x)		(cast(lu_mem, (x)<<10))

LUA_API int lua_getgcthreshold (lua_State *L) {
  int threshold;
  lua_lock(L);
  threshold = GCscale(G(L)->GCthreshold);
  lua_unlock(L);
  return threshold;
}

LUA_API int lua_getgccount (lua_State *L) {
  int count;
  lua_lock(L);
  count = GCscale(G(L)->nblocks);
  lua_unlock(L);
  return count;
}

LUA_API void lua_setgcthreshold (lua_State *L, int newthreshold) {
  lua_lock(L);
  if (newthreshold > GCscale(ULONG_MAX))
    G(L)->GCthreshold = ULONG_MAX;
  else
    G(L)->GCthreshold = GCunscale(newthreshold);
  luaC_checkGC(L);
  lua_unlock(L);
}


/*
** miscellaneous functions
*/


LUA_API void lua_error (lua_State *L, const char *s) {
  lua_lock(L);
  luaD_error(L, s);
  lua_unlock(L);
}


LUA_API int lua_next (lua_State *L, int index) {
  StkId t;
  lua_lock(L);
  t = luaA_index(L, index);
  api_check(L, ttype(t) == LUA_TTABLE);
  index = luaH_index(L, hvalue(t), luaA_index(L, -1));
  index = (luaH_nexti(hvalue(t), index, L->top - 1) != -1);
  if (index) {
    api_incr_top(L);
  }
  else  /* no more elements */
    L->top -= 1;  /* remove key */
  lua_unlock(L);
  return index;
}


LUA_API int lua_getn (lua_State *L, int index) {
  StkId t;
  const TObject *value;
  int n;
  lua_lock(L);
  t = luaA_index(L, index);
  api_check(L, ttype(t) == LUA_TTABLE);
  value = luaH_getstr(hvalue(t), luaS_newliteral(L, "n"));  /* = t.n */
  if (ttype(value) == LUA_TNUMBER)
    n = cast(int, nvalue(value));
  else {
    Node *nd;
    Table *a = hvalue(t);
    lua_Number max = 0;
    int i;
    i = sizearray(a);
    while (i--) {
      if (ttype(&a->array[i]) != LUA_TNIL)
        break;
    }
    max = i+1;
    i = sizenode(a);
    nd = a->node;
    while (i--) {
      if (ttype(key(nd)) == LUA_TNUMBER &&
          ttype(val(nd)) != LUA_TNIL &&
          nvalue(key(nd)) > max)
        max = nvalue(key(nd));
      nd++;
    }
    n = cast(int, max);
  }
  lua_unlock(L);
  return n;
}


LUA_API void lua_concat (lua_State *L, int n) {
  lua_lock(L);
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
  lua_unlock(L);
}


static Udata *pushnewudata (lua_State *L, size_t size) {
  Udata *u = luaS_newudata(L, size);
  setuvalue(L->top, u);
  api_incr_top(L);
  return uvalue(L->top-1);
}


LUA_API void *lua_newuserdata (lua_State *L, size_t size) {
  Udata *u;
  void *p;
  lua_lock(L);
  u = pushnewudata(L, size);
  p = u->uv.value;
  lua_unlock(L);
  return p;
}


LUA_API void lua_newuserdatabox (lua_State *L, void *p) {
  Udata *u;
  lua_lock(L);
  u = pushnewudata(L, 0);
  u->uv.value = p;
  lua_unlock(L);
}


LUA_API int lua_getweakmode (lua_State *L, int index) {
  StkId t;
  int mode;
  lua_lock(L);
  t = luaA_index(L, index);
  api_check(L, ttype(t) == LUA_TTABLE);
  mode = hvalue(t)->weakmode;
  lua_unlock(L);
  return mode;
}


LUA_API void  lua_setweakmode (lua_State *L, int mode) {
  lua_lock(L);
  api_check(L, ttype(L->top-1) == LUA_TTABLE);
  hvalue(L->top-1)->weakmode = cast(lu_byte, mode);
  lua_unlock(L);
}



LUA_API void lua_pushupvalues (lua_State *L) {
  TObject *func;
  int n, i;
  lua_lock(L);
  func = (L->ci->base - 1);
  api_check(L, iscfunction(func));
  n = clvalue(func)->c.nupvalues;
  if (LUA_MINSTACK+n > lua_stackspace(L))
    luaD_error(L, "stack overflow");
  for (i=0; i<n; i++) {
    setobj(L->top, &clvalue(func)->c.upvalue[i]);
    L->top++;
  }
  lua_unlock(L);
}


