/*
** $Id: lapi.c,v 1.235a 2003/04/07 14:36:08 roberto Exp $
** Lua API
** See Copyright Notice in lua.h
*/


#include <assert.h>
#include <string.h>

#define lapi_c

#include "lua.h"

#include "lapi.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lundump.h"
#include "lvm.h"


const char lua_ident[] =
  "$Lua: " LUA_VERSION " " LUA_COPYRIGHT " $\n"
  "$Authors: " LUA_AUTHORS " $\n"
  "$URL: www.lua.org $\n";



#ifndef api_check
#define api_check(L, o)		/*{ assert(o); }*/
#endif

#define api_checknelems(L, n)	api_check(L, (n) <= (L->top - L->base))

#define api_incr_top(L)   {api_check(L, L->top < L->ci->top); L->top++;}




static TObject *negindex (lua_State *L, int idx) {
  if (idx > LUA_REGISTRYINDEX) {
    api_check(L, idx != 0 && -idx <= L->top - L->base);
    return L->top+idx;
  }
  else switch (idx) {  /* pseudo-indices */
    case LUA_REGISTRYINDEX: return registry(L);
    case LUA_GLOBALSINDEX: return gt(L);
    default: {
      TObject *func = (L->base - 1);
      idx = LUA_GLOBALSINDEX - idx;
      lua_assert(iscfunction(func));
      return (idx <= clvalue(func)->c.nupvalues)
                ? &clvalue(func)->c.upvalue[idx-1]
                : NULL;
    }
  }
}


static TObject *luaA_index (lua_State *L, int idx) {
  if (idx > 0) {
    api_check(L, idx <= L->top - L->base);
    return L->base + idx - 1;
  }
  else {
    TObject *o = negindex(L, idx);
    api_check(L, o != NULL);
    return o;
  }
}


static TObject *luaA_indexAcceptable (lua_State *L, int idx) {
  if (idx > 0) {
    TObject *o = L->base+(idx-1);
    api_check(L, idx <= L->stack_last - L->base);
    if (o >= L->top) return NULL;
    else return o;
  }
  else
    return negindex(L, idx);
}


void luaA_pushobject (lua_State *L, const TObject *o) {
  setobj2s(L->top, o);
  incr_top(L);
}


LUA_API int lua_checkstack (lua_State *L, int size) {
  int res;
  lua_lock(L);
  if ((L->top - L->base + size) > LUA_MAXCSTACK)
    res = 0;  /* stack overflow */
  else {
    luaD_checkstack(L, size);
    if (L->ci->top < L->top + size)
      L->ci->top = L->top + size;
    res = 1;
  }
  lua_unlock(L);
  return res;
}


LUA_API void lua_xmove (lua_State *from, lua_State *to, int n) {
  int i;
  lua_lock(to);
  api_checknelems(from, n);
  from->top -= n;
  for (i = 0; i < n; i++) {
    setobj2s(to->top, from->top + i);
    api_incr_top(to);
  }
  lua_unlock(to);
}


LUA_API lua_CFunction lua_atpanic (lua_State *L, lua_CFunction panicf) {
  lua_CFunction old;
  lua_lock(L);
  old = G(L)->panic;
  G(L)->panic = panicf;
  lua_unlock(L);
  return old;
}


LUA_API lua_State *lua_newthread (lua_State *L) {
  lua_State *L1;
  lua_lock(L);
  luaC_checkGC(L);
  L1 = luaE_newthread(L);
  setthvalue(L->top, L1);
  api_incr_top(L);
  lua_unlock(L);
  lua_userstateopen(L1);
  return L1;
}



/*
** basic stack manipulation
*/


LUA_API int lua_gettop (lua_State *L) {
  return (L->top - L->base);
}


LUA_API void lua_settop (lua_State *L, int idx) {
  lua_lock(L);
  if (idx >= 0) {
    api_check(L, idx <= L->stack_last - L->base);
    while (L->top < L->base + idx)
      setnilvalue(L->top++);
    L->top = L->base + idx;
  }
  else {
    api_check(L, -(idx+1) <= (L->top - L->base));
    L->top += idx+1;  /* `subtract' index (index is negative) */
  }
  lua_unlock(L);
}


LUA_API void lua_remove (lua_State *L, int idx) {
  StkId p;
  lua_lock(L);
  p = luaA_index(L, idx);
  while (++p < L->top) setobjs2s(p-1, p);
  L->top--;
  lua_unlock(L);
}


LUA_API void lua_insert (lua_State *L, int idx) {
  StkId p;
  StkId q;
  lua_lock(L);
  p = luaA_index(L, idx);
  for (q = L->top; q>p; q--) setobjs2s(q, q-1);
  setobjs2s(p, L->top);
  lua_unlock(L);
}


LUA_API void lua_replace (lua_State *L, int idx) {
  lua_lock(L);
  api_checknelems(L, 1);
  setobj(luaA_index(L, idx), L->top - 1);  /* write barrier */
  L->top--;
  lua_unlock(L);
}


LUA_API void lua_pushvalue (lua_State *L, int idx) {
  lua_lock(L);
  setobj2s(L->top, luaA_index(L, idx));
  api_incr_top(L);
  lua_unlock(L);
}



/*
** access functions (stack -> C)
*/


LUA_API int lua_type (lua_State *L, int idx) {
  StkId o = luaA_indexAcceptable(L, idx);
  return (o == NULL) ? LUA_TNONE : ttype(o);
}


LUA_API const char *lua_typename (lua_State *L, int t) {
  UNUSED(L);
  return (t == LUA_TNONE) ? "no value" : luaT_typenames[t];
}


LUA_API int lua_iscfunction (lua_State *L, int idx) {
  StkId o = luaA_indexAcceptable(L, idx);
  return (o == NULL) ? 0 : iscfunction(o);
}


LUA_API int lua_isnumber (lua_State *L, int idx) {
  TObject n;
  const TObject *o = luaA_indexAcceptable(L, idx);
  return (o != NULL && tonumber(o, &n));
}


LUA_API int lua_isstring (lua_State *L, int idx) {
  int t = lua_type(L, idx);
  return (t == LUA_TSTRING || t == LUA_TNUMBER);
}


LUA_API int lua_isuserdata (lua_State *L, int idx) {
  const TObject *o = luaA_indexAcceptable(L, idx);
  return (o != NULL && (ttisuserdata(o) || ttislightuserdata(o)));
}


LUA_API int lua_rawequal (lua_State *L, int index1, int index2) {
  StkId o1 = luaA_indexAcceptable(L, index1);
  StkId o2 = luaA_indexAcceptable(L, index2);
  return (o1 == NULL || o2 == NULL) ? 0  /* index out of range */
                                    : luaO_rawequalObj(o1, o2);
}


LUA_API int lua_equal (lua_State *L, int index1, int index2) {
  StkId o1, o2;
  int i;
  lua_lock(L);  /* may call tag method */
  o1 = luaA_indexAcceptable(L, index1);
  o2 = luaA_indexAcceptable(L, index2);
  i = (o1 == NULL || o2 == NULL) ? 0  /* index out of range */
                                 : equalobj(L, o1, o2);
  lua_unlock(L);
  return i;
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



LUA_API lua_Number lua_tonumber (lua_State *L, int idx) {
  TObject n;
  const TObject *o = luaA_indexAcceptable(L, idx);
  if (o != NULL && tonumber(o, &n))
    return nvalue(o);
  else
    return 0;
}


LUA_API int lua_toboolean (lua_State *L, int idx) {
  const TObject *o = luaA_indexAcceptable(L, idx);
  return (o != NULL) && !l_isfalse(o);
}


LUA_API const char *lua_tostring (lua_State *L, int idx) {
  StkId o = luaA_indexAcceptable(L, idx);
  if (o == NULL)
    return NULL;
  else if (ttisstring(o))
    return svalue(o);
  else {
    const char *s;
    lua_lock(L);  /* `luaV_tostring' may create a new string */
    s = (luaV_tostring(L, o) ? svalue(o) : NULL);
    luaC_checkGC(L);
    lua_unlock(L);
    return s;
  }
}


LUA_API size_t lua_strlen (lua_State *L, int idx) {
  StkId o = luaA_indexAcceptable(L, idx);
  if (o == NULL)
    return 0;
  else if (ttisstring(o))
    return tsvalue(o)->tsv.len;
  else {
    size_t l;
    lua_lock(L);  /* `luaV_tostring' may create a new string */
    l = (luaV_tostring(L, o) ? tsvalue(o)->tsv.len : 0);
    lua_unlock(L);
    return l;
  }
}


LUA_API lua_CFunction lua_tocfunction (lua_State *L, int idx) {
  StkId o = luaA_indexAcceptable(L, idx);
  return (o == NULL || !iscfunction(o)) ? NULL : clvalue(o)->c.f;
}


LUA_API void *lua_touserdata (lua_State *L, int idx) {
  StkId o = luaA_indexAcceptable(L, idx);
  if (o == NULL) return NULL;
  switch (ttype(o)) {
    case LUA_TUSERDATA: return (uvalue(o) + 1);
    case LUA_TLIGHTUSERDATA: return pvalue(o);
    default: return NULL;
  }
}


LUA_API lua_State *lua_tothread (lua_State *L, int idx) {
  StkId o = luaA_indexAcceptable(L, idx);
  return (o == NULL || !ttisthread(o)) ? NULL : thvalue(o);
}


LUA_API const void *lua_topointer (lua_State *L, int idx) {
  StkId o = luaA_indexAcceptable(L, idx);
  if (o == NULL) return NULL;
  else {
    switch (ttype(o)) {
      case LUA_TTABLE: return hvalue(o);
      case LUA_TFUNCTION: return clvalue(o);
      case LUA_TTHREAD: return thvalue(o);
      case LUA_TUSERDATA:
      case LUA_TLIGHTUSERDATA:
        return lua_touserdata(L, idx);
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
  luaC_checkGC(L);
  setsvalue2s(L->top, luaS_newlstr(L, s, len));
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API void lua_pushstring (lua_State *L, const char *s) {
  if (s == NULL)
    lua_pushnil(L);
  else
    lua_pushlstring(L, s, strlen(s));
}


LUA_API const char *lua_pushvfstring (lua_State *L, const char *fmt,
                                      va_list argp) {
  const char *ret;
  lua_lock(L);
  luaC_checkGC(L);
  ret = luaO_pushvfstring(L, fmt, argp);
  lua_unlock(L);
  return ret;
}


LUA_API const char *lua_pushfstring (lua_State *L, const char *fmt, ...) {
  const char *ret;
  va_list argp;
  lua_lock(L);
  luaC_checkGC(L);
  va_start(argp, fmt);
  ret = luaO_pushvfstring(L, fmt, argp);
  va_end(argp);
  lua_unlock(L);
  return ret;
}


LUA_API void lua_pushcclosure (lua_State *L, lua_CFunction fn, int n) {
  Closure *cl;
  lua_lock(L);
  luaC_checkGC(L);
  api_checknelems(L, n);
  cl = luaF_newCclosure(L, n);
  cl->c.f = fn;
  L->top -= n;
  while (n--)
    setobj2n(&cl->c.upvalue[n], L->top+n);
  setclvalue(L->top, cl);
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API void lua_pushboolean (lua_State *L, int b) {
  lua_lock(L);
  setbvalue(L->top, (b != 0));  /* ensure that true is 1 */
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API void lua_pushlightuserdata (lua_State *L, void *p) {
  lua_lock(L);
  setpvalue(L->top, p);
  api_incr_top(L);
  lua_unlock(L);
}



/*
** get functions (Lua -> stack)
*/


LUA_API void lua_gettable (lua_State *L, int idx) {
  StkId t;
  lua_lock(L);
  t = luaA_index(L, idx);
  setobj2s(L->top - 1, luaV_gettable(L, t, L->top - 1, 0));
  lua_unlock(L);
}


LUA_API void lua_rawget (lua_State *L, int idx) {
  StkId t;
  lua_lock(L);
  t = luaA_index(L, idx);
  api_check(L, ttistable(t));
  setobj2s(L->top - 1, luaH_get(hvalue(t), L->top - 1));
  lua_unlock(L);
}


LUA_API void lua_rawgeti (lua_State *L, int idx, int n) {
  StkId o;
  lua_lock(L);
  o = luaA_index(L, idx);
  api_check(L, ttistable(o));
  setobj2s(L->top, luaH_getnum(hvalue(o), n));
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API void lua_newtable (lua_State *L) {
  lua_lock(L);
  luaC_checkGC(L);
  sethvalue(L->top, luaH_new(L, 0, 0));
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API int lua_getmetatable (lua_State *L, int objindex) {
  const TObject *obj;
  Table *mt = NULL;
  int res;
  lua_lock(L);
  obj = luaA_indexAcceptable(L, objindex);
  if (obj != NULL) {
    switch (ttype(obj)) {
      case LUA_TTABLE:
        mt = hvalue(obj)->metatable;
        break;
      case LUA_TUSERDATA:
        mt = uvalue(obj)->uv.metatable;
        break;
    }
  }
  if (mt == NULL || mt == hvalue(defaultmeta(L)))
    res = 0;
  else {
    sethvalue(L->top, mt);
    api_incr_top(L);
    res = 1;
  }
  lua_unlock(L);
  return res;
}


LUA_API void lua_getfenv (lua_State *L, int idx) {
  StkId o;
  lua_lock(L);
  o = luaA_index(L, idx);
  setobj2s(L->top, isLfunction(o) ? &clvalue(o)->l.g : gt(L));
  api_incr_top(L);
  lua_unlock(L);
}


/*
** set functions (stack -> Lua)
*/


LUA_API void lua_settable (lua_State *L, int idx) {
  StkId t;
  lua_lock(L);
  api_checknelems(L, 2);
  t = luaA_index(L, idx);
  luaV_settable(L, t, L->top - 2, L->top - 1);
  L->top -= 2;  /* pop index and value */
  lua_unlock(L);
}


LUA_API void lua_rawset (lua_State *L, int idx) {
  StkId t;
  lua_lock(L);
  api_checknelems(L, 2);
  t = luaA_index(L, idx);
  api_check(L, ttistable(t));
  setobj2t(luaH_set(L, hvalue(t), L->top-2), L->top-1);  /* write barrier */
  L->top -= 2;
  lua_unlock(L);
}


LUA_API void lua_rawseti (lua_State *L, int idx, int n) {
  StkId o;
  lua_lock(L);
  api_checknelems(L, 1);
  o = luaA_index(L, idx);
  api_check(L, ttistable(o));
  setobj2t(luaH_setnum(L, hvalue(o), n), L->top-1);  /* write barrier */
  L->top--;
  lua_unlock(L);
}


LUA_API int lua_setmetatable (lua_State *L, int objindex) {
  TObject *obj, *mt;
  int res = 1;
  lua_lock(L);
  api_checknelems(L, 1);
  obj = luaA_index(L, objindex);
  mt = (!ttisnil(L->top - 1)) ? L->top - 1 : defaultmeta(L);
  api_check(L, ttistable(mt));
  switch (ttype(obj)) {
    case LUA_TTABLE: {
      hvalue(obj)->metatable = hvalue(mt);  /* write barrier */
      break;
    }
    case LUA_TUSERDATA: {
      uvalue(obj)->uv.metatable = hvalue(mt);  /* write barrier */
      break;
    }
    default: {
      res = 0;  /* cannot set */
      break;
    }
  }
  L->top--;
  lua_unlock(L);
  return res;
}


LUA_API int lua_setfenv (lua_State *L, int idx) {
  StkId o;
  int res = 0;
  lua_lock(L);
  api_checknelems(L, 1);
  o = luaA_index(L, idx);
  L->top--;
  api_check(L, ttistable(L->top));
  if (isLfunction(o)) {
    res = 1;
    clvalue(o)->l.g = *(L->top);
  }
  lua_unlock(L);
  return res;
}


/*
** `load' and `call' functions (run Lua code)
*/

LUA_API void lua_call (lua_State *L, int nargs, int nresults) {
  StkId func;
  lua_lock(L);
  api_checknelems(L, nargs+1);
  func = L->top - (nargs+1);
  luaD_call(L, func, nresults);
  lua_unlock(L);
}



/*
** Execute a protected call.
*/
struct CallS {  /* data to `f_call' */
  StkId func;
  int nresults;
};


static void f_call (lua_State *L, void *ud) {
  struct CallS *c = cast(struct CallS *, ud);
  luaD_call(L, c->func, c->nresults);
}



LUA_API int lua_pcall (lua_State *L, int nargs, int nresults, int errfunc) {
  struct CallS c;
  int status;
  ptrdiff_t func;
  lua_lock(L);
  func = (errfunc == 0) ? 0 : savestack(L, luaA_index(L, errfunc));
  c.func = L->top - (nargs+1);  /* function to be called */
  c.nresults = nresults;
  status = luaD_pcall(L, f_call, &c, savestack(L, c.func), func);
  lua_unlock(L);
  return status;
}


/*
** Execute a protected C call.
*/
struct CCallS {  /* data to `f_Ccall' */
  lua_CFunction func;
  void *ud;
};


static void f_Ccall (lua_State *L, void *ud) {
  struct CCallS *c = cast(struct CCallS *, ud);
  Closure *cl;
  cl = luaF_newCclosure(L, 0);
  cl->c.f = c->func;
  setclvalue(L->top, cl);  /* push function */
  incr_top(L);
  setpvalue(L->top, c->ud);  /* push only argument */
  incr_top(L);
  luaD_call(L, L->top - 2, 0);
}


LUA_API int lua_cpcall (lua_State *L, lua_CFunction func, void *ud) {
  struct CCallS c;
  int status;
  lua_lock(L);
  c.func = func;
  c.ud = ud;
  status = luaD_pcall(L, f_Ccall, &c, savestack(L, L->top), 0);
  lua_unlock(L);
  return status;
}


LUA_API int lua_load (lua_State *L, lua_Chunkreader reader, void *data,
                      const char *chunkname) {
  ZIO z;
  int status;
  int c;
  lua_lock(L);
  if (!chunkname) chunkname = "?";
  luaZ_init(&z, reader, data, chunkname);
  c = luaZ_lookahead(&z);
  status = luaD_protectedparser(L, &z, (c == LUA_SIGNATURE[0]));
  lua_unlock(L);
  return status;
}


LUA_API int lua_dump (lua_State *L, lua_Chunkwriter writer, void *data) {
  int status;
  TObject *o;
  lua_lock(L);
  api_checknelems(L, 1);
  o = L->top - 1;
  if (isLfunction(o) && clvalue(o)->l.nupvalues == 0) {
    luaU_dump(L, clvalue(o)->l.p, writer, data);
    status = 1;
  }
  else
    status = 0;
  lua_unlock(L);
  return status;
}


/*
** Garbage-collection functions
*/

/* GC values are expressed in Kbytes: #bytes/2^10 */
#define GCscalel(x)		((x)>>10)
#define GCscale(x)		(cast(int, GCscalel(x)))
#define GCunscale(x)		(cast(lu_mem, x)<<10)

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
  if (cast(lu_mem, newthreshold) > GCscalel(MAX_LUMEM))
    G(L)->GCthreshold = MAX_LUMEM;
  else
    G(L)->GCthreshold = GCunscale(newthreshold);
  luaC_checkGC(L);
  lua_unlock(L);
}


/*
** miscellaneous functions
*/


LUA_API const char *lua_version (void) {
  return LUA_VERSION;
}


LUA_API int lua_error (lua_State *L) {
  lua_lock(L);
  api_checknelems(L, 1);
  luaG_errormsg(L);
  lua_unlock(L);
  return 0;  /* to avoid warnings */
}


LUA_API int lua_next (lua_State *L, int idx) {
  StkId t;
  int more;
  lua_lock(L);
  t = luaA_index(L, idx);
  api_check(L, ttistable(t));
  more = luaH_next(L, hvalue(t), L->top - 1);
  if (more) {
    api_incr_top(L);
  }
  else  /* no more elements */
    L->top -= 1;  /* remove key */
  lua_unlock(L);
  return more;
}


LUA_API void lua_concat (lua_State *L, int n) {
  lua_lock(L);
  luaC_checkGC(L);
  api_checknelems(L, n);
  if (n >= 2) {
    luaV_concat(L, n, L->top - L->base - 1);
    L->top -= (n-1);
  }
  else if (n == 0) {  /* push empty string */
    setsvalue2s(L->top, luaS_newlstr(L, NULL, 0));
    api_incr_top(L);
  }
  /* else n == 1; nothing to do */
  lua_unlock(L);
}


LUA_API void *lua_newuserdata (lua_State *L, size_t size) {
  Udata *u;
  lua_lock(L);
  luaC_checkGC(L);
  u = luaS_newudata(L, size);
  setuvalue(L->top, u);
  api_incr_top(L);
  lua_unlock(L);
  return u + 1;
}


LUA_API int lua_pushupvalues (lua_State *L) {
  Closure *func;
  int n, i;
  lua_lock(L);
  api_check(L, iscfunction(L->base - 1));
  func = clvalue(L->base - 1);
  n = func->c.nupvalues;
  luaD_checkstack(L, n + LUA_MINSTACK);
  for (i=0; i<n; i++) {
    setobj2s(L->top, &func->c.upvalue[i]);
    L->top++;
  }
  lua_unlock(L);
  return n;
}


static const char *aux_upvalue (lua_State *L, int funcindex, int n,
                                TObject **val) {
  Closure *f;
  StkId fi = luaA_index(L, funcindex);
  if (!ttisfunction(fi)) return NULL;
  f = clvalue(fi);
  if (f->c.isC) {
    if (!(1 <= n && n <= f->c.nupvalues)) return NULL;
    *val = &f->c.upvalue[n-1];
    return "";
  }
  else {
    Proto *p = f->l.p;
    if (!(1 <= n && n <= p->sizeupvalues)) return NULL;
    *val = f->l.upvals[n-1]->v;
    return getstr(p->upvalues[n-1]);
  }
}


LUA_API const char *lua_getupvalue (lua_State *L, int funcindex, int n) {
  const char *name;
  TObject *val;
  lua_lock(L);
  name = aux_upvalue(L, funcindex, n, &val);
  if (name) {
    setobj2s(L->top, val);
    api_incr_top(L);
  }
  lua_unlock(L);
  return name;
}


LUA_API const char *lua_setupvalue (lua_State *L, int funcindex, int n) {
  const char *name;
  TObject *val;
  lua_lock(L);
  api_checknelems(L, 1);
  name = aux_upvalue(L, funcindex, n, &val);
  if (name) {
    L->top--;
    setobj(val, L->top);  /* write barrier */
  }
  lua_unlock(L);
  return name;
}

