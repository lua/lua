/*
** $Id: lbaselib.c,v 1.68 2002/04/15 20:54:41 roberto Exp roberto $
** Basic library
** See Copyright Notice in lua.h
*/



#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"

#include "lauxlib.h"
#include "luadebug.h"
#include "lualib.h"




/*
** If your system does not support `stderr', redefine this function, or
** redefine _ERRORMESSAGE so that it won't need _ALERT.
*/
static int luaB__ALERT (lua_State *L) {
  fputs(luaL_check_string(L, 1), stderr);
  return 0;
}


/*
** Basic implementation of _ERRORMESSAGE.
** The library `liolib' redefines _ERRORMESSAGE for better error information.
*/
static int luaB__ERRORMESSAGE (lua_State *L) {
  luaL_check_type(L, 1, LUA_TSTRING);
  lua_getglobal(L, LUA_ALERT);
  if (lua_isfunction(L, -1)) {  /* avoid error loop if _ALERT is not defined */
    lua_Debug ar;
    lua_pushliteral(L, "error: ");
    lua_pushvalue(L, 1);
    if (lua_getstack(L, 1, &ar)) {
      lua_getinfo(L, "Sl", &ar);
      if (ar.source && ar.currentline > 0) {
        char buff[100];
        sprintf(buff, "\n  <%.70s: line %d>", ar.short_src, ar.currentline);
        lua_pushstring(L, buff);
        lua_concat(L, 2);
      }
    }
    lua_pushliteral(L, "\n");
    lua_concat(L, 3);
    lua_rawcall(L, 1, 0);
  }
  return 0;
}


/*
** If your system does not support `stdout', you can just remove this function.
** If you need, you can define your own `print' function, following this
** model but changing `fputs' to put the strings at a proper place
** (a console window or a log file, for instance).
*/
static int luaB_print (lua_State *L) {
  int n = lua_gettop(L);  /* number of arguments */
  int i;
  lua_getglobal(L, "tostring");
  for (i=1; i<=n; i++) {
    const char *s;
    lua_pushvalue(L, -1);  /* function to be called */
    lua_pushvalue(L, i);   /* value to print */
    lua_rawcall(L, 1, 1);
    s = lua_tostring(L, -1);  /* get result */
    if (s == NULL)
      lua_error(L, "`tostring' must return a string to `print'");
    if (i>1) fputs("\t", stdout);
    fputs(s, stdout);
    lua_pop(L, 1);  /* pop result */
  }
  fputs("\n", stdout);
  return 0;
}


static int luaB_tonumber (lua_State *L) {
  int base = luaL_opt_int(L, 2, 10);
  if (base == 10) {  /* standard conversion */
    luaL_check_any(L, 1);
    if (lua_isnumber(L, 1)) {
      lua_pushnumber(L, lua_tonumber(L, 1));
      return 1;
    }
  }
  else {
    const char *s1 = luaL_check_string(L, 1);
    char *s2;
    unsigned long n;
    luaL_arg_check(L, 2 <= base && base <= 36, 2, "base out of range");
    n = strtoul(s1, &s2, base);
    if (s1 != s2) {  /* at least one valid digit? */
      while (isspace((unsigned char)(*s2))) s2++;  /* skip trailing spaces */
      if (*s2 == '\0') {  /* no invalid trailing characters? */
        lua_pushnumber(L, n);
        return 1;
      }
    }
  }
  lua_pushnil(L);  /* else not a number */
  return 1;
}


static int luaB_error (lua_State *L) {
  lua_error(L, luaL_opt_string(L, 1, NULL));
  return 0;  /* to avoid warnings */
}


static int luaB_metatable (lua_State *L) {
  luaL_check_any(L, 1);
  if (lua_isnone(L, 2)) {
    if (!lua_getmetatable(L, 1))
      return 0;  /* no metatable */
    else {
      lua_pushliteral(L, "__metatable");
      lua_rawget(L, -2);
      if (lua_isnil(L, -1))
        lua_pop(L, 1);
    }
  }
  else {
    int t = lua_type(L, 2);
    luaL_check_type(L, 1, LUA_TTABLE);
    luaL_arg_check(L, t == LUA_TNIL || t == LUA_TTABLE, 2, "nil/table expected");
    lua_settop(L, 2);
    lua_setmetatable(L, 1);
  }
  return 1;
}


static int luaB_globals (lua_State *L) {
  lua_getglobals(L);  /* value to be returned */
  if (!lua_isnone(L, 1)) {
    luaL_check_type(L, 1, LUA_TTABLE);
    lua_pushvalue(L, 1);  /* new table of globals */
    lua_setglobals(L);
  }
  return 1;
}

static int luaB_rawget (lua_State *L) {
  luaL_check_type(L, 1, LUA_TTABLE);
  luaL_check_any(L, 2);
  lua_rawget(L, 1);
  return 1;
}

static int luaB_rawset (lua_State *L) {
  luaL_check_type(L, 1, LUA_TTABLE);
  luaL_check_any(L, 2);
  luaL_check_any(L, 3);
  lua_rawset(L, 1);
  return 1;
}


static int luaB_gcinfo (lua_State *L) {
  lua_pushnumber(L, lua_getgccount(L));
  lua_pushnumber(L, lua_getgcthreshold(L));
  return 2;
}


static int luaB_collectgarbage (lua_State *L) {
  lua_setgcthreshold(L, luaL_opt_int(L, 1, 0));
  return 0;
}


static int luaB_type (lua_State *L) {
  luaL_check_any(L, 1);
  lua_pushstring(L, lua_typename(L, lua_type(L, 1)));
  return 1;
}


static int luaB_next (lua_State *L) {
  luaL_check_type(L, 1, LUA_TTABLE);
  lua_settop(L, 2);  /* create a 2nd argument if there isn't one */
  if (lua_next(L, 1))
    return 2;
  else {
    lua_pushnil(L);
    return 1;
  }
}


static int luaB_nexti (lua_State *L) {
  lua_Number i = lua_tonumber(L, 2);
  luaL_check_type(L, 1, LUA_TTABLE);
  if (i == 0) {  /* `for' start? */
    lua_getglobal(L, "nexti");  /* return generator, */
    lua_pushvalue(L, 1);  /* state, */
    lua_pushnumber(L, 1);  /* and initial value */
    return 3;
  }
  else {  /* `for' step */
    lua_pushnumber(L, i+1);  /* next value */
    lua_rawgeti(L, 1, i);
    return (lua_isnil(L, -1)) ? 0 : 2;
  }
}


static int passresults (lua_State *L, int status, int oldtop) {
  if (status == 0) {
    int nresults = lua_gettop(L) - oldtop;
    if (nresults > 0)
      return nresults;  /* results are already on the stack */
    else {
      lua_pushboolean(L, 1); /* at least one result to signal no errors */
      return 1;
    }
  }
  else {  /* error */
    lua_pushnil(L);
    lua_pushstring(L, luaL_errstr(status));  /* error code */
    return 2;
  }
}


static int luaB_dostring (lua_State *L) {
  int oldtop = lua_gettop(L);
  size_t l;
  const char *s = luaL_check_lstr(L, 1, &l);
  const char *chunkname = luaL_opt_string(L, 2, s);
  return passresults(L, lua_dobuffer(L, s, l, chunkname), oldtop);
}


static int luaB_loadstring (lua_State *L) {
  int oldtop = lua_gettop(L);
  size_t l;
  const char *s = luaL_check_lstr(L, 1, &l);
  const char *chunkname = luaL_opt_string(L, 2, s);
  return passresults(L, lua_loadbuffer(L, s, l, chunkname), oldtop);
}


static int luaB_dofile (lua_State *L) {
  int oldtop = lua_gettop(L);
  const char *fname = luaL_opt_string(L, 1, NULL);
  return passresults(L, lua_dofile(L, fname), oldtop);
}


static int luaB_loadfile (lua_State *L) {
  int oldtop = lua_gettop(L);
  const char *fname = luaL_opt_string(L, 1, NULL);
  return passresults(L, lua_loadfile(L, fname), oldtop);
}


static int luaB_assert (lua_State *L) {
  luaL_check_any(L, 1);
  if (!lua_toboolean(L, 1))
    luaL_verror(L, "assertion failed!  %.90s", luaL_opt_string(L, 2, ""));
  lua_settop(L, 1);
  return 1;
}


static int aux_unpack (lua_State *L, int arg) {
  int n, i;
  luaL_check_type(L, arg, LUA_TTABLE);
  n = lua_getn(L, arg);
  luaL_check_stack(L, n+LUA_MINSTACK, "table too big to unpack");
  for (i=1; i<=n; i++)  /* push arg[1...n] */
    lua_rawgeti(L, arg, i);
  return n;
}


static int luaB_unpack (lua_State *L) {
  return aux_unpack(L, 1);
}


static int luaB_call (lua_State *L) {
  int oldtop;
  const char *options = luaL_opt_string(L, 3, "");
  int err = 0;  /* index of old error method */
  int status;
  int n;
  if (!lua_isnone(L, 4)) {  /* set new error method */
    lua_getglobal(L, "_ERRORMESSAGE");
    err = lua_gettop(L);  /* get index */
    lua_pushvalue(L, 4);
    lua_setglobal(L, "_ERRORMESSAGE");
  }
  oldtop = lua_gettop(L);  /* top before function-call preparation */
  /* push function */
  lua_pushvalue(L, 1);
  n = aux_unpack(L, 2);  /* push arg[1...n] */
  status = lua_call(L, n, LUA_MULTRET);
  if (err != 0) {  /* restore old error method */
    lua_pushvalue(L, err);
    lua_setglobal(L, "_ERRORMESSAGE");
  }
  if (status != 0) {  /* error in call? */
    if (strchr(options, 'x'))
      lua_pushnil(L);  /* return nil to signal the error */
    else
      lua_error(L, NULL);  /* propagate error without additional messages */
    return 1;
  }
  if (strchr(options, 'p'))  /* pack results? */
    lua_error(L, "obsolete option `p' in `call'");
  return lua_gettop(L) - oldtop;  /* results are already on the stack */
}


static int luaB_tostring (lua_State *L) {
  char buff[64];
  luaL_checkany(L, 1);
  if (luaL_callmeta(L, 1, "__tostring"))  /* is there a metafield? */
    return 1;  /* use its value */
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER:
      lua_pushstring(L, lua_tostring(L, 1));
      return 1;
    case LUA_TSTRING:
      lua_pushvalue(L, 1);
      return 1;
    case LUA_TBOOLEAN:
      lua_pushstring(L, (lua_toboolean(L, 1) ? "true" : "false"));
      return 1;
    case LUA_TTABLE:
      sprintf(buff, "table: %p", lua_topointer(L, 1));
      break;
    case LUA_TFUNCTION:
      sprintf(buff, "function: %p", lua_topointer(L, 1));
      break;
    case LUA_TUSERDATA:
    case LUA_TUDATAVAL:
      sprintf(buff, "userdata: %p", lua_touserdata(L, 1));
      break;
    case LUA_TNIL:
      lua_pushliteral(L, "nil");
      return 1;
  }
  lua_pushstring(L, buff);
  return 1;
}



/*
** {======================================================
** `require' function
** =======================================================
*/


/* name of global that holds table with loaded packages */
#define REQTAB		"_LOADED"

/* name of global that holds the search path for packages */
#define LUA_PATH	"LUA_PATH"

#ifndef LUA_PATH_SEP
#define LUA_PATH_SEP	';'
#endif

#ifndef LUA_PATH_DEFAULT
#define LUA_PATH_DEFAULT	"?.lua"
#endif


static const char *getpath (lua_State *L) {
  const char *path;
  lua_getglobal(L, LUA_PATH);  /* try global variable */
  path = lua_tostring(L, -1);
  if (path) return path;
  path = getenv(LUA_PATH);  /* else try environment variable */
  if (path) return path;
  return LUA_PATH_DEFAULT;  /* else use default */
}


static const char *nextpath (lua_State *L, const char *path) {
  const char *l;
  if (*path == '\0') return NULL;  /* no more pathes */
  if (*path == LUA_PATH_SEP) path++;  /* skip separator */
  l = strchr(path, LUA_PATH_SEP);  /* find next separator */
  if (l == NULL) l = path+strlen(path);
  lua_pushlstring(L, path, l - path);  /* directory name */
  return l;
}


static void composename (lua_State *L) {
  const char *path = lua_tostring(L, -1);
  const char *wild = strchr(path, '?');
  if (wild == NULL) return;  /* no wild char; path is the file name */
  lua_pushlstring(L, path, wild - path);
  lua_pushvalue(L, 1);  /* package name */
  lua_pushstring(L, wild + 1);
  lua_concat(L, 3);
}


static int luaB_require (lua_State *L) {
  const char *path;
  int status = LUA_ERRFILE;  /* not found (yet) */
  luaL_check_string(L, 1);
  lua_settop(L, 1);
  lua_pushvalue(L, 1);
  lua_setglobal(L, "_REQUIREDNAME");
  lua_getglobal(L, REQTAB);
  if (!lua_istable(L, 2)) lua_error(L, REQTAB " is not a table");
  path = getpath(L);
  lua_pushvalue(L, 1);  /* check package's name in book-keeping table */
  lua_gettable(L, 2);
  if (!lua_isnil(L, -1))  /* is it there? */
    return 0;  /* package is already loaded */
  else {  /* must load it */
    while (status == LUA_ERRFILE && (path = nextpath(L, path)) != NULL) {
      composename(L);
      status = lua_dofile(L, lua_tostring(L, -1));  /* try to load it */
      lua_settop(L, 3);  /* pop string and eventual results from dofile */
    }
  }
  switch (status) {
    case 0: {
      lua_pushvalue(L, 1);
      lua_pushboolean(L, 1);
      lua_settable(L, 2);  /* mark it as loaded */
      return 0;
    }
    case LUA_ERRFILE: {  /* file not found */
      luaL_verror(L, "could not load package `%.20s' from path `%.200s'",
                  lua_tostring(L, 1), lua_tostring(L, 3));
    }
    default: {  /* error loading package */
      lua_error(L, NULL);
      return 0;  /* to avoid warnings */
    }
  }
}

/* }====================================================== */


static const luaL_reg base_funcs[] = {
  {LUA_ALERT, luaB__ALERT},
  {"_ERRORMESSAGE", luaB__ERRORMESSAGE},
  {"error", luaB_error},
  {"metatable", luaB_metatable},
  {"globals", luaB_globals},
  {"next", luaB_next},
  {"nexti", luaB_nexti},
  {"print", luaB_print},
  {"tonumber", luaB_tonumber},
  {"tostring", luaB_tostring},
  {"type", luaB_type},
  {"assert", luaB_assert},
  {"unpack", luaB_unpack},
  {"rawget", luaB_rawget},
  {"rawset", luaB_rawset},
  {"call", luaB_call},
  {"collectgarbage", luaB_collectgarbage},
  {"gcinfo", luaB_gcinfo},
  {"loadfile", luaB_loadfile},
  {"loadstring", luaB_loadstring},
  {"dofile", luaB_dofile},
  {"dostring", luaB_dostring},
  {"require", luaB_require},
  {NULL, NULL}
};


/*
** {======================================================
** Coroutine library
** =======================================================
*/


static int luaB_resume (lua_State *L) {
  lua_State *co = (lua_State *)lua_getfrombox(L, lua_upvalueindex(1));
  lua_settop(L, 0);
  if (lua_resume(L, co) != 0)
    lua_error(L, "error running co-routine");
  return lua_gettop(L);
}



static int gc_coroutine (lua_State *L) {
  lua_State *co = (lua_State *)lua_getfrombox(L, 1);
  lua_closethread(L, co);
  return 0;
}


static int luaB_coroutine (lua_State *L) {
  lua_State *NL;
  int ref;
  int i;
  int n = lua_gettop(L);
  luaL_arg_check(L, lua_isfunction(L, 1) && !lua_iscfunction(L, 1), 1,
    "Lua function expected");
  NL = lua_newthread(L);
  if (NL == NULL) lua_error(L, "unable to create new thread");
  /* move function and arguments from L to NL */
  for (i=0; i<n; i++) {
    ref = lua_ref(L, 1);
    lua_getref(NL, ref);
    lua_insert(NL, 1);
    lua_unref(L, ref);
  }
  lua_cobegin(NL, n-1);
  lua_newpointerbox(L, NL);
  lua_pushliteral(L, "Coroutine");
  lua_rawget(L, LUA_REGISTRYINDEX);
  lua_setmetatable(L, -2);
  lua_pushcclosure(L, luaB_resume, 1);
  return 1;
}


static int luaB_yield (lua_State *L) {
  return lua_yield(L, lua_gettop(L));
}

static const luaL_reg co_funcs[] = {
  {"create", luaB_coroutine},
  {"yield", luaB_yield},
  {NULL, NULL}
};


static void co_open (lua_State *L) {
  luaL_opennamedlib(L, "co", co_funcs, 0);
  /* create metatable for coroutines */
  lua_pushliteral(L, "Coroutine");
  lua_newtable(L);
  lua_pushliteral(L, "__gc");
  lua_pushcfunction(L, gc_coroutine);
  lua_rawset(L, -3);
  lua_rawset(L, LUA_REGISTRYINDEX);
}

/* }====================================================== */



static void base_open (lua_State *L) {
  lua_pushliteral(L, "_G");
  lua_pushvalue(L, LUA_GLOBALSINDEX);
  luaL_openlib(L, base_funcs, 0);  /* open lib into global table */
  lua_pushliteral(L, "_VERSION");
  lua_pushliteral(L, LUA_VERSION);
  lua_settable(L, -3);  /* set global _VERSION */
  lua_settable(L, -1);  /* set global _G */
}


LUALIB_API int lua_baselibopen (lua_State *L) {
  base_open(L);
  co_open(L);
  lua_newtable(L);
  lua_setglobal(L, REQTAB);
  return 0;
}

