/*
** $Id: lapi.c,v 1.187 2002/05/02 16:55:55 roberto Exp roberto $
** Lua API
** See Copyright Notice in lua.h
*/


#include <stdio.h>
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
#include "lundump.h"
#include "lvm.h"


const char lua_ident[] =
  "$Lua: " LUA_VERSION " " LUA_COPYRIGHT " $\n"
  "$Authors: " LUA_AUTHORS " $\n"
  "$URL: www.lua.org $\n";


#ifndef lua_filerror
#include <errno.h>
#define lua_fileerror	(strerror(errno))
#endif


#ifndef api_check
#define api_check(L, o)		((void)1)
#endif

#define api_checknelems(L, n)	api_check(L, (n) <= (L->top - L->ci->base))

#define api_incr_top(L)   (api_check(L, L->top<L->ci->top), L->top++)




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


#define luaA_index(L, index) \
  ( (index > 0) ? \
    (api_check(L, index <= L->top - L->ci->base), L->ci->base+index-1) : \
    negindex(L, index))


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
  incr_top(L);
}


LUA_API int lua_checkstack (lua_State *L, int size) {
  int res;
  lua_lock(L);
  if ((L->top - L->ci->base + size) > LUA_MAXCSTACK)
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


LUA_API lua_CFunction lua_setpanicf (lua_State *L, lua_CFunction panicf) {
  lua_CFunction old;
  lua_lock(L);
  old = G(L)->panic;
  G(L)->panic = panicf;
  lua_unlock(L);
  return old;
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
    while (L->top < L->ci->base + index)
      setnilvalue(L->top++);
    L->top = L->ci->base + index;
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


LUA_API void lua_replace (lua_State *L, int index) {
  lua_lock(L);
  api_checknelems(L, 1);
  setobj(luaA_index(L, index), L->top - 1);
  L->top--;
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
  const TObject *o = luaA_indexAcceptable(L, index);
  return (o != NULL && tonumber(o, &n));
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
                                 : luaV_cmp(L, o1, o2, CMP_LT);
  lua_unlock(L);
  return i;
}



LUA_API lua_Number lua_tonumber (lua_State *L, int index) {
  TObject n;
  const TObject *o = luaA_indexAcceptable(L, index);
  if (o != NULL && tonumber(o, &n))
    return nvalue(o);
  else
    return 0;
}


LUA_API int lua_toboolean (lua_State *L, int index) {
  const TObject *o = luaA_indexAcceptable(L, index);
  return (o != NULL) && !l_isfalse(o);
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
    s = (luaV_tostring(L, o) ? svalue(o) : NULL);
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
    l = (luaV_tostring(L, o) ? tsvalue(o)->tsv.len : 0);
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
  if (o == NULL) return NULL;
  switch (ttype(o)) {
    case LUA_TUSERDATA: return (uvalue(o) + 1);
    case LUA_TUDATAVAL: return pvalue(o);
    default: return NULL;
  }
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
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API void lua_pushboolean (lua_State *L, int b) {
  lua_lock(L);
  setbvalue(L->top, (b != 0));  /* ensure that true is 1 */
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API void lua_pushudataval (lua_State *L, void *p) {
  lua_lock(L);
  setpvalue(L->top, p);
  api_incr_top(L);
  lua_unlock(L);
}



/*
** get functions (Lua -> stack)
*/


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


LUA_API int lua_getmetatable (lua_State *L, int objindex) {
  StkId obj;
  Table *mt;
  int res;
  lua_lock(L);
  obj = luaA_indexAcceptable(L, objindex);
  switch (ttype(obj)) {
    case LUA_TTABLE:
      mt = hvalue(obj)->metatable;
      break;
    case LUA_TUSERDATA:
      mt = uvalue(obj)->uv.metatable;
      break;
    default:
      mt = hvalue(defaultmeta(L));
  }
  if (mt == hvalue(defaultmeta(L)))
    res = 0;
  else {
    sethvalue(L->top, mt);
    api_incr_top(L);
    res = 1;
  }
  lua_unlock(L);
  return res;
}


/*
** set functions (stack -> Lua)
*/


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


LUA_API void lua_setmetatable (lua_State *L, int objindex) {
  StkId obj, mt;
  lua_lock(L);
  api_checknelems(L, 1);
  obj = luaA_index(L, objindex);
  mt = --L->top;
  if (ttype(mt) == LUA_TNIL)
    mt = defaultmeta(L);
  api_check(L, ttype(mt) == LUA_TTABLE);
  switch (ttype(obj)) {
    case LUA_TTABLE:
      hvalue(obj)->metatable = hvalue(mt);
      break;
    case LUA_TUSERDATA:
      uvalue(obj)->uv.metatable = hvalue(mt);
      break;
    default:
      luaO_verror(L, "cannot change the meta table of a %.20s",
                  luaT_typenames[ttype(obj)]);
  }
  lua_unlock(L);
}


/*
** `load' and `call' functions (run Lua code)
*/

LUA_API void lua_rawcall (lua_State *L, int nargs, int nresults) {
  StkId func;
  lua_lock(L);
  api_checknelems(L, nargs+1);
  func = L->top - (nargs+1);
  luaD_call(L, func, nresults);
  lua_unlock(L);
}


LUA_API int lua_pcall (lua_State *L, int nargs, int nresults, int errf) {
  int status;
  const TObject *err;
  lua_lock(L);
  err = (errf == 0) ? &luaO_nilobject : luaA_index(L, errf);
  status = luaD_pcall(L, nargs, nresults, err);
  lua_unlock(L);
  return status;
}


static int errfile (lua_State *L, const char *filename) {
  if (filename == NULL) filename = "stdin";
  lua_pushliteral(L, "cannot read ");
  lua_pushstring(L, filename);
  lua_pushliteral(L, ": ");
  lua_pushstring(L, lua_fileerror);
  lua_concat(L, 4);
  return LUA_ERRFILE;
}


LUA_API int lua_loadfile (lua_State *L, const char *filename) {
  ZIO z;
  const char *luafname;  /* name used by lua */ 
  int status;
  int bin;  /* flag for file mode */
  int nlevel;  /* level on the stack of filename */
  FILE *f = (filename == NULL) ? stdin : fopen(filename, "r");
  if (f == NULL) return errfile(L, filename);  /* unable to open file */
  bin = (ungetc(getc(f), f) == LUA_SIGNATURE[0]);
  if (bin && f != stdin) {
    fclose(f);
    f = fopen(filename, "rb");  /* reopen in binary mode */
    if (f == NULL) return errfile(L, filename);  /* unable to reopen file */
  }
  if (filename == NULL)
    lua_pushstring(L, "=stdin");
  else {
    lua_pushliteral(L, "@");
    lua_pushstring(L, filename);
    lua_concat(L, 2);
  }
  nlevel = lua_gettop(L);
  luafname = lua_tostring(L, -1);  /* luafname = `@'..filename */
  luaZ_Fopen(&z, f, luafname);
  status = luaD_protectedparser(L, &z, bin);
  if (ferror(f))
    return errfile(L, filename);
  lua_remove(L, nlevel);  /* remove filename */
  if (f != stdin)
    fclose(f);
  return status;
}


LUA_API int lua_loadbuffer (lua_State *L, const char *buff, size_t size,
                          const char *name) {
  ZIO z;
  if (!name) name = "?";
  luaZ_mopen(&z, buff, size, name);
  return luaD_protectedparser(L, &z, buff[0]==LUA_SIGNATURE[0]);
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


LUA_API void lua_errorobj (lua_State *L) {
  lua_lock(L);
  api_checknelems(L, 1);
  luaD_errorobj(L, L->top - 1, LUA_ERRRUN);
  lua_unlock(L);
}


LUA_API int lua_next (lua_State *L, int index) {
  StkId t;
  int more;
  lua_lock(L);
  t = luaA_index(L, index);
  api_check(L, ttype(t) == LUA_TTABLE);
  more = luaH_next(L, hvalue(t), L->top - 1);
  if (more) {
    api_incr_top(L);
  }
  else  /* no more elements */
    L->top -= 1;  /* remove key */
  lua_unlock(L);
  return more;
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
    lua_number2int(n, nvalue(value));
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
    lua_number2int(n, max);
  }
  lua_unlock(L);
  return n;
}


LUA_API void lua_concat (lua_State *L, int n) {
  lua_lock(L);
  api_checknelems(L, n);
  if (n >= 2) {
    luaV_strconc(L, n, L->top - L->ci->base - 1);
    L->top -= (n-1);
    luaC_checkGC(L);
  }
  else if (n == 0) {  /* push empty string */
    setsvalue(L->top, luaS_newlstr(L, NULL, 0));
    api_incr_top(L);
  }
  /* else n == 1; nothing to do */
  lua_unlock(L);
}


LUA_API void *lua_newuserdata (lua_State *L, size_t size) {
  Udata *u;
  lua_lock(L);
  u = luaS_newudata(L, size);
  setuvalue(L->top, u);
  api_incr_top(L);
  lua_unlock(L);
  return u + 1;
}


LUA_API int lua_pushupvalues (lua_State *L) {
  TObject *func;
  int n, i;
  lua_lock(L);
  func = (L->ci->base - 1);
  api_check(L, iscfunction(func));
  n = clvalue(func)->c.nupvalues;
  luaD_checkstack(L, n + LUA_MINSTACK);
  for (i=0; i<n; i++) {
    setobj(L->top, &clvalue(func)->c.upvalue[i]);
    L->top++;
  }
  lua_unlock(L);
  return n;
}



/*
** {======================================================
** compatibility code
** =======================================================
*/


static void callalert (lua_State *L, int status) {
  if (status != 0) {
    int top = lua_gettop(L);
    lua_getglobal(L, "_ALERT");
    lua_insert(L, -2);
    lua_pcall(L, 1, 0, 0);
    lua_settop(L, top-1);
  }
}


LUA_API int lua_call (lua_State *L, int nargs, int nresults) {
  int status;
  int errpos = lua_gettop(L) - nargs;
  lua_getglobal(L, "_ERRORMESSAGE");
  lua_insert(L, errpos);  /* put below function and args */
  status = lua_pcall(L, nargs, nresults, errpos);
  lua_remove(L, errpos);
  callalert(L, status);
  return status;
}

static int aux_do (lua_State *L, int status) {
  if (status == 0) {  /* parse OK? */
    int err = lua_gettop(L);
    lua_getglobal(L, "_ERRORMESSAGE");
    lua_insert(L, err);
    status = lua_pcall(L, 0, LUA_MULTRET, err);  /* call main */
    lua_remove(L, err);  /* remove error function */
  }
  callalert(L, status);
  return status;
}


LUA_API int lua_dofile (lua_State *L, const char *filename) {
  return aux_do(L, lua_loadfile(L, filename));
}


LUA_API int lua_dobuffer (lua_State *L, const char *buff, size_t size,
                          const char *name) {
  return aux_do(L, lua_loadbuffer(L, buff, size, name));
}


LUA_API int lua_dostring (lua_State *L, const char *str) {
  return lua_dobuffer(L, str, strlen(str), str);
}

/* }====================================================== */
