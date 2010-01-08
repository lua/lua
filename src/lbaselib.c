/*
** $Id: lbaselib.c,v 1.235 2009/12/28 16:30:31 roberto Exp $
** Basic library
** See Copyright Notice in lua.h
*/



#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define lbaselib_c
#define LUA_LIB

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"


static int luaB_print (lua_State *L) {
  int n = lua_gettop(L);  /* number of arguments */
  int i;
  lua_getfield(L, LUA_ENVIRONINDEX, "tostring");
  for (i=1; i<=n; i++) {
    const char *s;
    size_t l;
    lua_pushvalue(L, -1);  /* function to be called */
    lua_pushvalue(L, i);   /* value to print */
    lua_call(L, 1, 1);
    s = lua_tolstring(L, -1, &l);  /* get result */
    if (s == NULL)
      return luaL_error(L, LUA_QL("tostring") " must return a string to "
                           LUA_QL("print"));
    if (i>1) luai_writestring("\t", 1);
    luai_writestring(s, l);
    lua_pop(L, 1);  /* pop result */
  }
  luai_writestring("\n", 1);
  return 0;
}


static int luaB_tonumber (lua_State *L) {
  int base = luaL_optint(L, 2, 10);
  if (base == 10) {  /* standard conversion */
    luaL_checkany(L, 1);
    if (lua_isnumber(L, 1)) {
      lua_pushnumber(L, lua_tonumber(L, 1));
      return 1;
    }
  }
  else {
    const char *s1 = luaL_checkstring(L, 1);
    char *s2;
    unsigned long n;
    luaL_argcheck(L, 2 <= base && base <= 36, 2, "base out of range");
    n = strtoul(s1, &s2, base);
    if (s1 != s2) {  /* at least one valid digit? */
      while (isspace((unsigned char)(*s2))) s2++;  /* skip trailing spaces */
      if (*s2 == '\0') {  /* no invalid trailing characters? */
        lua_pushnumber(L, (lua_Number)n);
        return 1;
      }
    }
  }
  lua_pushnil(L);  /* else not a number */
  return 1;
}


static int luaB_error (lua_State *L) {
  int level = luaL_optint(L, 2, 1);
  lua_settop(L, 1);
  if (lua_isstring(L, 1) && level > 0) {  /* add extra information? */
    luaL_where(L, level);
    lua_pushvalue(L, 1);
    lua_concat(L, 2);
  }
  return lua_error(L);
}


static int luaB_getmetatable (lua_State *L) {
  luaL_checkany(L, 1);
  if (!lua_getmetatable(L, 1)) {
    lua_pushnil(L);
    return 1;  /* no metatable */
  }
  luaL_getmetafield(L, 1, "__metatable");
  return 1;  /* returns either __metatable field (if present) or metatable */
}


static int luaB_setmetatable (lua_State *L) {
  int t = lua_type(L, 2);
  luaL_checktype(L, 1, LUA_TTABLE);
  luaL_argcheck(L, t == LUA_TNIL || t == LUA_TTABLE, 2,
                    "nil or table expected");
  if (luaL_getmetafield(L, 1, "__metatable"))
    return luaL_error(L, "cannot change a protected metatable");
  lua_settop(L, 2);
  lua_setmetatable(L, 1);
  return 1;
}



#if defined(LUA_COMPAT_FENV)

static void getfunc (lua_State *L, int opt) {
  if (lua_isfunction(L, 1)) lua_pushvalue(L, 1);
  else {
    lua_Debug ar;
    int level = opt ? luaL_optint(L, 1, 1) : luaL_checkint(L, 1);
    luaL_argcheck(L, level >= 0, 1, "level must be non-negative");
    if (lua_getstack(L, level, &ar) == 0)
      luaL_argerror(L, 1, "invalid level");
    lua_getinfo(L, "f", &ar);
  }
}

static int luaB_getfenv (lua_State *L) {
  getfunc(L, 1);
  if (lua_iscfunction(L, -1))  /* is a C function? */
    lua_pushglobaltable(L);  /* return the global env. */
  else
    lua_getfenv(L, -1);
  return 1;
}

static int luaB_setfenv (lua_State *L) {
  luaL_checktype(L, 2, LUA_TTABLE);
  getfunc(L, 0);
  lua_pushvalue(L, 2);
  if (lua_iscfunction(L, -2) || lua_setfenv(L, -2) == 0)
    return luaL_error(L,
             LUA_QL("setfenv") " cannot change environment of given object");
  return 1;
}

#else

static int luaB_getfenv (lua_State *L) {
  return luaL_error(L, "getfenv/setfenv deprecated");
}

#define luaB_setfenv	luaB_getfenv

#endif


static int luaB_rawequal (lua_State *L) {
  luaL_checkany(L, 1);
  luaL_checkany(L, 2);
  lua_pushboolean(L, lua_rawequal(L, 1, 2));
  return 1;
}


static int luaB_rawget (lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  luaL_checkany(L, 2);
  lua_settop(L, 2);
  lua_rawget(L, 1);
  return 1;
}

static int luaB_rawset (lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  luaL_checkany(L, 2);
  luaL_checkany(L, 3);
  lua_settop(L, 3);
  lua_rawset(L, 1);
  return 1;
}


static int luaB_gcinfo (lua_State *L) {
  lua_pushinteger(L, lua_gc(L, LUA_GCCOUNT, 0));
  return 1;
}


static int luaB_collectgarbage (lua_State *L) {
  static const char *const opts[] = {"stop", "restart", "collect",
    "count", "step", "setpause", "setstepmul", "isrunning", NULL};
  static const int optsnum[] = {LUA_GCSTOP, LUA_GCRESTART, LUA_GCCOLLECT,
    LUA_GCCOUNT, LUA_GCSTEP, LUA_GCSETPAUSE, LUA_GCSETSTEPMUL,
    LUA_GCISRUNNING};
  int o = optsnum[luaL_checkoption(L, 1, "collect", opts)];
  int ex = luaL_optint(L, 2, 0);
  int res = lua_gc(L, o, ex);
  switch (o) {
    case LUA_GCCOUNT: {
      int b = lua_gc(L, LUA_GCCOUNTB, 0);
      lua_pushnumber(L, res + ((lua_Number)b/1024));
      lua_pushinteger(L, b);
      return 2;
    }
    case LUA_GCSTEP: case LUA_GCISRUNNING: {
      lua_pushboolean(L, res);
      return 1;
    }
    default: {
      lua_pushinteger(L, res);
      return 1;
    }
  }
}


static int luaB_type (lua_State *L) {
  luaL_checkany(L, 1);
  lua_pushstring(L, luaL_typename(L, 1));
  return 1;
}


static int pairsmeta (lua_State *L, const char *method, int iszero) {
  if (!luaL_getmetafield(L, 1, method)) {  /* no metamethod? */
    luaL_checktype(L, 1, LUA_TTABLE);  /* argument must be a table */
    lua_pushvalue(L, lua_upvalueindex(1));  /* will return generator, */
    lua_pushvalue(L, 1);  /* state, */
    if (iszero) lua_pushinteger(L, 0);  /* and initial value */
    else lua_pushnil(L);
  }
  else {
    lua_pushvalue(L, 1);  /* argument 'self' to metamethod */
    lua_call(L, 1, 3);  /* get 3 values from metamethod */
  }
  return 3;
}


static int luaB_next (lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  lua_settop(L, 2);  /* create a 2nd argument if there isn't one */
  if (lua_next(L, 1))
    return 2;
  else {
    lua_pushnil(L);
    return 1;
  }
}


static int luaB_pairs (lua_State *L) {
  return pairsmeta(L, "__pairs", 0);
}


static int ipairsaux (lua_State *L) {
  int i = luaL_checkint(L, 2);
  luaL_checktype(L, 1, LUA_TTABLE);
  i++;  /* next value */
  lua_pushinteger(L, i);
  lua_rawgeti(L, 1, i);
  return (lua_isnil(L, -1) && i > luaL_len(L, 1)) ? 0 : 2;
}


static int luaB_ipairs (lua_State *L) {
  return pairsmeta(L, "__ipairs", 1);
}


static int load_aux (lua_State *L, int status) {
  if (status == LUA_OK)
    return 1;
  else {
    lua_pushnil(L);
    lua_insert(L, -2);  /* put before error message */
    return 2;  /* return nil plus error message */
  }
}


static int luaB_loadfile (lua_State *L) {
  const char *fname = luaL_optstring(L, 1, NULL);
  return load_aux(L, luaL_loadfile(L, fname));
}


/*
** {======================================================
** Generic Read function
** =======================================================
*/

static const char *checkrights (lua_State *L, const char *mode, const char *s) {
  if (strchr(mode, 'b') == NULL && *s == LUA_SIGNATURE[0])
    return lua_pushstring(L, "attempt to load a binary chunk");
  if (strchr(mode, 't') == NULL && *s != LUA_SIGNATURE[0])
    return lua_pushstring(L, "attempt to load a text chunk");
  return NULL;  /* chunk in allowed format */
}


/*
** reserves a slot, above all arguments, to hold a copy of the returned
** string to avoid it being collected while parsed
*/
#define RESERVEDSLOT	4


/*
** Reader for generic `load' function: `lua_load' uses the
** stack for internal stuff, so the reader cannot change the
** stack top. Instead, it keeps its resulting string in a
** reserved slot inside the stack.
*/
typedef struct {  /* reader state */
  int f;  /* position of reader function on stack */
  const char *mode;  /* allowed modes (binary/text) */
} Readstat;

static const char *generic_reader (lua_State *L, void *ud, size_t *size) {
  const char *s;
  Readstat *stat = (Readstat *)ud;
  luaL_checkstack(L, 2, "too many nested functions");
  lua_pushvalue(L, stat->f);  /* get function */
  lua_call(L, 0, 1);  /* call it */
  if (lua_isnil(L, -1)) {
    *size = 0;
    return NULL;
  }
  else if ((s = lua_tostring(L, -1)) != NULL) {
    if (stat->mode != NULL) {  /* first time? */
      s = checkrights(L, stat->mode, s);  /* check mode */
      stat->mode = NULL;  /* to avoid further checks */
      if (s) luaL_error(L, s);
    }
    lua_replace(L, RESERVEDSLOT);  /* save string in reserved slot */
    return lua_tolstring(L, RESERVEDSLOT, size);
  }
  else {
    luaL_error(L, "reader function must return a string");
    return NULL;  /* to avoid warnings */
  }
}


static int luaB_load_aux (lua_State *L, int farg) {
  int status;
  Readstat stat;
  size_t l;
  const char *s = lua_tolstring(L, farg, &l);
  stat.mode = luaL_optstring(L, farg + 2, "bt");
  if (s != NULL) {  /* loading a string? */
    const char *chunkname = luaL_optstring(L, farg + 1, s);
    status = (checkrights(L, stat.mode, s) != NULL)
           || luaL_loadbuffer(L, s, l, chunkname);
  }
  else {  /* loading from a reader function */
    const char *chunkname = luaL_optstring(L, farg + 1, "=(load)");
    luaL_checktype(L, farg, LUA_TFUNCTION);
    stat.f = farg;
    lua_settop(L, RESERVEDSLOT);  /* create reserved slot */
    status = lua_load(L, generic_reader, &stat, chunkname);
  }
  return load_aux(L, status);
}


static int luaB_load (lua_State *L) {
  return luaB_load_aux(L, 1);
}


static int luaB_loadin (lua_State *L) {
  int n;
  luaL_checktype(L, 1, LUA_TTABLE);
  n = luaB_load_aux(L, 2);
  if (n == 1) {  /* success? */
    lua_pushvalue(L, 1);  /* environment for loaded function */
    lua_setfenv(L, -2);
  }
  return n;
}


static int luaB_loadstring (lua_State *L) {
  lua_settop(L, 2);
  lua_pushliteral(L, "tb");
  return luaB_load(L);  /* dostring(s, n) == load(s, n, "tb") */

}
/* }====================================================== */


static int dofilecont (lua_State *L) {
  return lua_gettop(L) - 1;
}


static int luaB_dofile (lua_State *L) {
  const char *fname = luaL_optstring(L, 1, NULL);
  lua_settop(L, 1);
  if (luaL_loadfile(L, fname) != LUA_OK) lua_error(L);
  lua_callk(L, 0, LUA_MULTRET, 0, dofilecont);
  return dofilecont(L);
}


static int luaB_assert (lua_State *L) {
  if (!lua_toboolean(L, 1))
    return luaL_error(L, "%s", luaL_optstring(L, 2, "assertion failed!"));
  return lua_gettop(L);
}


static int luaB_select (lua_State *L) {
  int n = lua_gettop(L);
  if (lua_type(L, 1) == LUA_TSTRING && *lua_tostring(L, 1) == '#') {
    lua_pushinteger(L, n-1);
    return 1;
  }
  else {
    int i = luaL_checkint(L, 1);
    if (i < 0) i = n + i;
    else if (i > n) i = n;
    luaL_argcheck(L, 1 <= i, 1, "index out of range");
    return n - i;
  }
}


static int pcallcont (lua_State *L) {
  int errfunc;  /* call has an error function in bottom of the stack */
  int status = lua_getctx(L, &errfunc);
  lua_assert(status != LUA_OK);
  lua_pushboolean(L, (status == LUA_YIELD));  /* first result (status) */
  if (errfunc)  /* came from xpcall? */
    lua_replace(L, 1);  /* put first result in place of error function */
  else  /* came from pcall */
    lua_insert(L, 1);  /* open space for first result */
  return lua_gettop(L);
}


static int luaB_pcall (lua_State *L) {
  int status;
  luaL_checkany(L, 1);
  status = lua_pcallk(L, lua_gettop(L) - 1, LUA_MULTRET, 0, 0, pcallcont);
  luaL_checkstack(L, 1, NULL);
  lua_pushboolean(L, (status == LUA_OK));
  lua_insert(L, 1);
  return lua_gettop(L);  /* return status + all results */
}


static int luaB_xpcall (lua_State *L) {
  int status;
  int n = lua_gettop(L);
  luaL_argcheck(L, n >= 2, 2, "value expected");
  lua_pushvalue(L, 1);  /* exchange function... */
  lua_copy(L, 2, 1);  /* ...and error handler */
  lua_replace(L, 2);
  status = lua_pcallk(L, n - 2, LUA_MULTRET, 1, 1, pcallcont);
  luaL_checkstack(L, 1, NULL);
  lua_pushboolean(L, (status == LUA_OK));
  lua_replace(L, 1);
  return lua_gettop(L);  /* return status + all results */
}


static int luaB_tostring (lua_State *L) {
  luaL_checkany(L, 1);
  luaL_tolstring(L, 1, NULL);
  return 1;
}


static int luaB_newproxy (lua_State *L) {
  lua_settop(L, 1);
  lua_newuserdata(L, 0);  /* create proxy */
  if (lua_toboolean(L, 1) == 0)
    return 1;  /* no metatable */
  else if (lua_isboolean(L, 1)) {
    lua_createtable(L, 0, 1);  /* create a new metatable `m' ... */
    lua_pushboolean(L, 1);
    lua_setfield(L, -2, "__gc");  /* ... m.__gc = false (HACK!!)... */
    lua_pushvalue(L, -1);  /* ... and mark `m' as a valid metatable */
    lua_pushboolean(L, 1);
    lua_rawset(L, lua_upvalueindex(1));  /* weaktable[m] = true */
  }
  else {
    int validproxy = 0;  /* to check if weaktable[metatable(u)] == true */
    if (lua_getmetatable(L, 1)) {
      lua_rawget(L, lua_upvalueindex(1));
      validproxy = lua_toboolean(L, -1);
      lua_pop(L, 1);  /* remove value */
    }
    luaL_argcheck(L, validproxy, 1, "boolean or proxy expected");
    lua_getmetatable(L, 1);  /* metatable is valid; get it */
  }
  lua_setmetatable(L, 2);
  return 1;
}


static const luaL_Reg base_funcs[] = {
  {"assert", luaB_assert},
  {"collectgarbage", luaB_collectgarbage},
  {"dofile", luaB_dofile},
  {"error", luaB_error},
  {"gcinfo", luaB_gcinfo},
  {"getfenv", luaB_getfenv},
  {"getmetatable", luaB_getmetatable},
  {"loadfile", luaB_loadfile},
  {"load", luaB_load},
  {"loadin", luaB_loadin},
  {"loadstring", luaB_loadstring},
  {"next", luaB_next},
  {"pcall", luaB_pcall},
  {"print", luaB_print},
  {"rawequal", luaB_rawequal},
  {"rawget", luaB_rawget},
  {"rawset", luaB_rawset},
  {"select", luaB_select},
  {"setfenv", luaB_setfenv},
  {"setmetatable", luaB_setmetatable},
  {"tonumber", luaB_tonumber},
  {"tostring", luaB_tostring},
  {"type", luaB_type},
  {"xpcall", luaB_xpcall},
  {NULL, NULL}
};


/*
** {======================================================
** Coroutine library
** =======================================================
*/

static int auxresume (lua_State *L, lua_State *co, int narg) {
  int status;
  if (!lua_checkstack(co, narg)) {
    lua_pushliteral(L, "too many arguments to resume");
    return -1;  /* error flag */
  }
  if (lua_status(co) == LUA_OK && lua_gettop(co) == 0) {
    lua_pushliteral(L, "cannot resume dead coroutine");
    return -1;  /* error flag */
  }
  lua_xmove(L, co, narg);
  status = lua_resume(co, narg);
  if (status == LUA_OK || status == LUA_YIELD) {
    int nres = lua_gettop(co);
    if (!lua_checkstack(L, nres + 1)) {
      lua_pop(co, nres);  /* remove results anyway */
      lua_pushliteral(L, "too many results to resume");
      return -1;  /* error flag */
    }
    lua_xmove(co, L, nres);  /* move yielded values */
    return nres;
  }
  else {
    lua_xmove(co, L, 1);  /* move error message */
    return -1;  /* error flag */
  }
}


static int luaB_coresume (lua_State *L) {
  lua_State *co = lua_tothread(L, 1);
  int r;
  luaL_argcheck(L, co, 1, "coroutine expected");
  r = auxresume(L, co, lua_gettop(L) - 1);
  if (r < 0) {
    lua_pushboolean(L, 0);
    lua_insert(L, -2);
    return 2;  /* return false + error message */
  }
  else {
    lua_pushboolean(L, 1);
    lua_insert(L, -(r + 1));
    return r + 1;  /* return true + `resume' returns */
  }
}


static int luaB_auxwrap (lua_State *L) {
  lua_State *co = lua_tothread(L, lua_upvalueindex(1));
  int r = auxresume(L, co, lua_gettop(L));
  if (r < 0) {
    if (lua_isstring(L, -1)) {  /* error object is a string? */
      luaL_where(L, 1);  /* add extra info */
      lua_insert(L, -2);
      lua_concat(L, 2);
    }
    lua_error(L);  /* propagate error */
  }
  return r;
}


static int luaB_cocreate (lua_State *L) {
  lua_State *NL = lua_newthread(L);
  luaL_checktype(L, 1, LUA_TFUNCTION);
  lua_pushvalue(L, 1);  /* move function to top */
  lua_xmove(L, NL, 1);  /* move function from L to NL */
  return 1;
}


static int luaB_cowrap (lua_State *L) {
  luaB_cocreate(L);
  lua_pushcclosure(L, luaB_auxwrap, 1);
  return 1;
}


static int luaB_yield (lua_State *L) {
  return lua_yield(L, lua_gettop(L));
}


static int luaB_costatus (lua_State *L) {
  lua_State *co = lua_tothread(L, 1);
  luaL_argcheck(L, co, 1, "coroutine expected");
  if (L == co) lua_pushliteral(L, "running");
  else {
    switch (lua_status(co)) {
      case LUA_YIELD:
        lua_pushliteral(L, "suspended");
        break;
      case LUA_OK: {
        lua_Debug ar;
        if (lua_getstack(co, 0, &ar) > 0)  /* does it have frames? */
          lua_pushliteral(L, "normal");  /* it is running */
        else if (lua_gettop(co) == 0)
            lua_pushliteral(L, "dead");
        else
          lua_pushliteral(L, "suspended");  /* initial state */
        break;
      }
      default:  /* some error occurred */
        lua_pushliteral(L, "dead");
        break;
    }
  }
  return 1;
}


static int luaB_corunning (lua_State *L) {
  int ismain = lua_pushthread(L);
  lua_pushboolean(L, ismain);
  return 2;
}


static const luaL_Reg co_funcs[] = {
  {"create", luaB_cocreate},
  {"resume", luaB_coresume},
  {"running", luaB_corunning},
  {"status", luaB_costatus},
  {"wrap", luaB_cowrap},
  {"yield", luaB_yield},
  {NULL, NULL}
};

/* }====================================================== */


static void auxopen (lua_State *L, const char *name,
                     lua_CFunction f, lua_CFunction u) {
  lua_pushcfunction(L, u);
  lua_pushcclosure(L, f, 1);
  lua_setfield(L, -2, name);
}


static void base_open (lua_State *L) {
  /* set global _G */
  lua_pushglobaltable(L);
  lua_setfield(L, LUA_ENVIRONINDEX, "_G");
  /* open lib into global table */
  luaL_register(L, "_G", base_funcs);
  lua_pushliteral(L, LUA_VERSION);
  lua_setfield(L, LUA_ENVIRONINDEX, "_VERSION");  /* set global _VERSION */
  /* `ipairs' and `pairs' need auxiliary functions as upvalues */
  auxopen(L, "ipairs", luaB_ipairs, ipairsaux);
  auxopen(L, "pairs", luaB_pairs, luaB_next);
  /* `newproxy' needs a weaktable as upvalue */
  lua_createtable(L, 0, 1);  /* new table `w' */
  lua_pushvalue(L, -1);  /* `w' will be its own metatable */
  lua_setmetatable(L, -2);
  lua_pushliteral(L, "kv");
  lua_setfield(L, -2, "__mode");  /* metatable(w).__mode = "kv" */
  lua_pushcclosure(L, luaB_newproxy, 1);
  lua_setfield(L, LUA_ENVIRONINDEX, "newproxy");  /* set global `newproxy' */
}


LUAMOD_API int luaopen_base (lua_State *L) {
  base_open(L);
  luaL_register(L, LUA_COLIBNAME, co_funcs);
  return 2;
}

