/*
** $Id: loadlib.c,v 1.18 2005/02/28 15:58:48 roberto Exp roberto $
** Dynamic library loader for Lua
** See Copyright Notice in lua.h
*
* This module contains an implementation of loadlib for Unix systems
* that have dlfcn, an implementation for Darwin (Mac OS X), an
* implementation for Windows, and a stub for other systems.
*/


#include <stdlib.h>
#include <string.h>


#define loadlib_c
#define LUA_LIB

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"


#define LIBPREFIX	"LOADLIB: "

#define POF		LUA_POF
#define LIB_FAIL	"open"


static void ll_unloadlib (void *lib);
static void *ll_load (lua_State *L, const char *path);
static lua_CFunction ll_sym (lua_State *L, void *lib, const char *sym);



#if defined(USE_DLOPEN)
/*
** {========================================================================
** This is an implementation of loadlib based on the dlfcn interface.
** The dlfcn interface is available in Linux, SunOS, Solaris, IRIX, FreeBSD,
** NetBSD, AIX 4.2, HPUX 11, and  probably most other Unix flavors, at least
** as an emulation layer on top of native functions.
** =========================================================================
*/

#include <dlfcn.h>

static void ll_unloadlib (void *lib) {
  dlclose(lib);
}


static void *ll_load (lua_State *L, const char *path) {
  void *lib = dlopen(path, RTLD_NOW);
  if (lib == NULL) lua_pushstring(L, dlerror());
  return lib;
}


static lua_CFunction ll_sym (lua_State *L, void *lib, const char *sym) {
  lua_CFunction f = (lua_CFunction)dlsym(lib, sym);
  if (f == NULL) lua_pushstring(L, dlerror());
  return f;
}

/* }====================================================== */



#elif defined(USE_DLL)
/*
** {======================================================================
** This is an implementation of loadlib for Windows using native functions.
** =======================================================================
*/

#include <windows.h>


static void pusherror (lua_State *L) {
  int error = GetLastError();
  char buffer[128];
  if (FormatMessage(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
      NULL, error, 0, buffer, sizeof(buffer), NULL))
    lua_pushstring(L,buffer);
  else
    lua_pushfstring(L, "system error %d\n", error);
}

static void ll_unloadlib (void *lib) {
  FreeLibrary((HINSTANCE)lib);
}


static void *ll_load (lua_State *L, const char *path) {
  HINSTANCE lib = LoadLibrary(path);
  if (lib == NULL) pusherror(L);
  return lib;
}


static lua_CFunction ll_sym (lua_State *L, void *lib, const char *sym) {
  lua_CFunction f = (lua_CFunction)GetProcAddress((HINSTANCE)lib, sym);
  if (f == NULL) pusherror(L);
  return f;
}

/* }====================================================== */



#elif defined(USE_DYLD)
/*
** {======================================================================
** Native Mac OS X / Darwin Implementation
** =======================================================================
*/

#include <mach-o/dyld.h>


#undef POF
#define POF	"_" LUA_POF


static void pusherror (lua_State *L) {
  const char *err_str;
  const char *err_file;
  NSLinkEditErrors err;
  int err_num;
  NSLinkEditError(&err, &err_num, &err_file, &err_str);
  lua_pushstring(L, err_str);
}


static const char *errorfromcode (NSObjectFileImageReturnCode ret) {
  switch (ret) {
    case NSObjectFileImageInappropriateFile:
      return "file is not a bundle";
    case NSObjectFileImageArch:
      return "library is for wrong CPU type";
    case NSObjectFileImageFormat:
      return "bad format";
    case NSObjectFileImageAccess:
      return "cannot access file";
    case NSObjectFileImageFailure:
    default:
      return "unable to load library";
  }
}


static void ll_unloadlib (void *lib) {
  NSUnLinkModule((NSModule)lib, NSUNLINKMODULE_OPTION_RESET_LAZY_REFERENCES);
}


static void *ll_load (lua_State *L, const char *path) {
  NSObjectFileImage img;
  NSObjectFileImageReturnCode ret;
  /* this would be a rare case, but prevents crashing if it happens */
  if(!_dyld_present()) {
    lua_pushliteral(L, "dyld not present");
    return NULL;
  }
  ret = NSCreateObjectFileImageFromFile(path, &img);
  if (ret == NSObjectFileImageSuccess) {
    NSModule mod = NSLinkModule(img, path, NSLINKMODULE_OPTION_PRIVATE |
                       NSLINKMODULE_OPTION_RETURN_ON_ERROR);
    NSDestroyObjectFileImage(img);
    if (mod == NULL) pusherror(L);
    return mod;
  }
  lua_pushstring(L, errorfromcode(ret));
  return NULL;
}


static lua_CFunction ll_sym (lua_State *L, void *lib, const char *sym) {
  NSSymbol nss = NSLookupSymbolInModule((NSModule)lib, sym);
  if (nss == NULL) {
    lua_pushfstring(L, "symbol `%s' not found", sym);
    return NULL;
  }
  return (lua_CFunction)NSAddressOfSymbol(nss);
}

/* }====================================================== */



#else
/*
** {======================================================
** Fallback for other systems
** =======================================================
*/

#undef LIB_FAIL
#define LIB_FAIL	"absent"


static void ll_unloadlib (void *lib) {
  (void)lib;  /* to avoid warnings */
}


static void *ll_load (lua_State *L, const char *path) {
  (void)path;  /* to avoid warnings */
  lua_pushliteral(L,"`loadlib' not supported");
  return NULL;
}


static lua_CFunction ll_sym (lua_State *L, void *lib, const char *sym) {
  (void)lib; (void)sym;  /* to avoid warnings */
  lua_pushliteral(L,"`loadlib' not supported");
  return NULL;
}

/* }====================================================== */
#endif



static void **ll_register (lua_State *L, const char *path) {
  void **plib;
  lua_pushfstring(L, "%s%s", LIBPREFIX, path);
  lua_gettable(L, LUA_REGISTRYINDEX);  /* check library in registry? */
  if (!lua_isnil(L, -1))  /* is there an entry? */
    plib = (void **)lua_touserdata(L, -1);
  else {  /* no entry yet; create one */
    lua_pop(L, 1);
    plib = (void **)lua_newuserdata(L, sizeof(const void *));
    *plib = NULL;
    luaL_getmetatable(L, "_LOADLIB");
    lua_setmetatable(L, -2);
    lua_pushfstring(L, "%s%s", LIBPREFIX, path);
    lua_pushvalue(L, -2);
    lua_settable(L, LUA_REGISTRYINDEX);
  }
  return plib;
}


/*
** __gc tag method: calls library's `ll_unloadlib' function with the lib
** handle
*/
static int gctm (lua_State *L) {
  void **lib = (void **)luaL_checkudata(L, 1, "_LOADLIB");
  if (lib) {
    if (*lib) ll_unloadlib(*lib);
    *lib = NULL;  /* mark library as closed */
  }
  return 0;
}


static int ll_loadfunc (lua_State *L, const char *path, const char *sym) {
  const char *reason;
  void **reg = ll_register(L, path);
  if (*reg == NULL) *reg = ll_load(L, path);
  if (*reg == NULL)
    reason = LIB_FAIL;
  else {
    lua_CFunction f = ll_sym(L, *reg, sym);
    if (f) {
      lua_pushcfunction(L, f);
      return 1;  /* return function */
    }
    reason = "init";
  }
  lua_pushnil(L);
  lua_insert(L, -2);
  lua_pushstring(L, reason);
  return 3;  /* return nil, ll_error, reason */
}


static int ll_loadlib (lua_State *L) {
  const char *path = luaL_checkstring(L, 1);
  const char *init = luaL_checkstring(L, 2);
  return ll_loadfunc(L, path, init);
}



/*
** {======================================================
** `require' and `module' functions
** =======================================================
*/


static int loader_Lua (lua_State *L) {
  const char *name = luaL_checkstring(L, 1);
  const char *fname = luaL_gsub(L, name, ".", LUA_DIRSEP);
  const char *path;
  /* try first `LUA_PATH' for compatibility */
  lua_pushstring(L, "LUA_PATH");
  lua_rawget(L, LUA_GLOBALSINDEX);
  path = lua_tostring(L, -1);
  if (!path) {
    lua_pop(L, 1);
    lua_getfield(L, LUA_ENVIRONINDEX, "path");
    path = lua_tostring(L, -1);
  }
  if (path == NULL)
    luaL_error(L, "`package.path' must be a string");
  fname = luaL_searchpath(L, fname, path);
  if (fname == NULL) return 0;  /* library not found in this path */
  if (luaL_loadfile(L, fname) != 0)
    luaL_error(L, "error loading package `%s' (%s)", name, lua_tostring(L, -1));
  return 1;  /* library loaded successfully */
}


static int loader_C (lua_State *L) {
  const char *name = luaL_checkstring(L, 1);
  const char *fname = luaL_gsub(L, name, ".", LUA_DIRSEP);
  const char *path;
  const char *funcname;
  lua_getfield(L, LUA_ENVIRONINDEX, "cpath");
  path = lua_tostring(L, -1);
  if (path == NULL)
    luaL_error(L, "`package.cpath' must be a string");
  fname = luaL_searchpath(L, fname, path);
  if (fname == NULL) return 0;  /* library not found in this path */
  funcname = luaL_gsub(L, name, ".", LUA_OFSEP);
  funcname = lua_pushfstring(L, "%s%s", POF, funcname);
  if (ll_loadfunc(L, fname, funcname) != 1)
    luaL_error(L, "error loading package `%s' (%s)", name, lua_tostring(L, -2));
  return 1;  /* library loaded successfully */
}


static int loader_preload (lua_State *L) {
  lua_getfield(L, LUA_ENVIRONINDEX, "preload");
  if (!lua_istable(L, -1))
    luaL_error(L, "`package.preload' must be a table");
  lua_getfield(L, -1, luaL_checkstring(L, 1));
  return 1;
}


static int ll_require (lua_State *L) {
  const char *name = luaL_checkstring(L, 1);
  int i;
  lua_settop(L, 1);
  lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
  lua_getfield(L, 2, name);
  if (lua_toboolean(L, -1))  /* is it there? */
    return 1;  /* package is already loaded; return its result */
  /* else must load it; first mark it as loaded */
  lua_pushboolean(L, 1);
  lua_setfield(L, 2, name);  /* _LOADED[name] = true */
  /* iterate over available loaders */
  lua_getfield(L, LUA_ENVIRONINDEX, "loaders");
  if (!lua_istable(L, -1))
    luaL_error(L, "`package.loaders' must be a table");
  for (i=1;; i++) {
    lua_rawgeti(L, -1, i);  /* get a loader */
    if (lua_isnil(L, -1))
      return luaL_error(L, "package `%s' not found", name);
    lua_pushstring(L, name);
    lua_call(L, 1, 1);  /* call it */
    if (lua_isnil(L, -1)) lua_pop(L, 1);
    else break;  /* module loaded successfully */
  }
  lua_pushvalue(L, 1);  /* pass name as argument to module */
  lua_call(L, 1, 1);  /* run loaded module */
  if (!lua_isnil(L, -1))  /* non-nil return? */
    lua_setfield(L, 2, name);  /* update _LOADED[name] with returned value */
  lua_getfield(L, 2, name);  /* return _LOADED[name] */
  return 1;
}


static void setfenv (lua_State *L) {
  lua_Debug ar;
  lua_getstack(L, 1, &ar);
  lua_getinfo(L, "f", &ar);
  lua_pushvalue(L, -2);
  lua_setfenv(L, -2);
}


static int ll_module (lua_State *L) {
  const char *modname = luaL_checkstring(L, 1);
  const char *dot;
  lua_settop(L, 1);
  lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
  /* try to find given table */
  luaL_getfield(L, LUA_GLOBALSINDEX, modname);
  if (lua_isnil(L, -1)) {  /* not found? */
    lua_pop(L, 1);  /* remove previous result */
    lua_newtable(L);  /* create it */
    /* register it with given name */
    lua_pushvalue(L, -1);
    luaL_setfield(L, LUA_GLOBALSINDEX, modname);
  }
  else if (!lua_istable(L, -1))
    return luaL_error(L, "name conflict for module `%s'", modname);
  /* check whether table already has a _NAME field */
  lua_getfield(L, -1, "_NAME");
  if (!lua_isnil(L, -1))  /* is table an initialized module? */
    lua_pop(L, 1);
  else {  /* no; initialize it */
    lua_pop(L, 1);
    lua_newtable(L);  /* create new metatable */
    lua_pushvalue(L, LUA_GLOBALSINDEX);
    lua_setfield(L, -2, "__index");  /* mt.__index = _G */
    lua_setmetatable(L, -2);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "_M");  /* module._M = module */
    lua_pushstring(L, modname);
    lua_setfield(L, -2, "_NAME");
    dot = strrchr(modname, '.');  /* look for last dot in module name */
    if (dot == NULL) dot = modname;
    else dot++;
    /* set _PACKAGE as package name (full module name minus last part) */
    lua_pushlstring(L, modname, dot - modname);
    lua_setfield(L, -2, "_PACKAGE");
  }
  lua_pushvalue(L, -1);
  lua_setfield(L, 2, modname);  /* _LOADED[modname] = new table */
  setfenv(L);
  return 0;
}


/* }====================================================== */


static const luaL_reg ll_funcs[] = {
  {"require", ll_require},
  {"module", ll_module},
  {NULL, NULL}
};


static const lua_CFunction loaders[] =
  {loader_preload, loader_C, loader_Lua, NULL};


LUALIB_API int luaopen_loadlib (lua_State *L) {
  const char *path;
  int i;
  /* create new type _LOADLIB */
  luaL_newmetatable(L, "_LOADLIB");
  lua_pushcfunction(L, gctm);
  lua_setfield(L, -2, "__gc");
  /* create `package' table */
  lua_newtable(L);
  lua_pushvalue(L, -1);
  lua_setglobal(L, "package");
  lua_pushvalue(L, -1);
  lua_setfield(L, LUA_REGISTRYINDEX, "_PACKAGE");
  lua_pushvalue(L, -1);
  lua_replace(L, LUA_ENVIRONINDEX);
  /* create `loaders' table */
  lua_newtable(L);
  /* fill it with pre-defined loaders */
  for (i=0; loaders[i] != NULL; i++) {
    lua_pushcfunction(L, loaders[i]);
    lua_rawseti(L, -2, i+1);
  }
  /* put it in field `loaders' */
  lua_setfield(L, -2, "loaders");
  /* set field `path' */
  path = getenv(LUA_PATH);
  if (path == NULL) path = LUA_PATH_DEFAULT;
  lua_pushstring(L, path);
  lua_setfield(L, -2, "path");
  /* set field `cpath' */
  path = getenv(LUA_CPATH);
  if (path == NULL) path = LUA_CPATH_DEFAULT;
  lua_pushstring(L, path);
  lua_setfield(L, -2, "cpath");
  /* set field `loaded' */
  lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
  lua_setfield(L, -2, "loaded");
  /* set field `preload' */
  lua_newtable(L);
  lua_setfield(L, -2, "preload");
  /* create `loadlib' function */
  lua_pushcfunction(L, ll_loadlib);
  lua_pushvalue(L, -1);
  lua_setfield(L, LUA_GLOBALSINDEX, "loadlib");  /* COMPATIBILITY ONLY!! */
  lua_setfield(L, -2, "loadlib");
  lua_pushvalue(L, LUA_GLOBALSINDEX);
  luaL_openlib(L, NULL, ll_funcs, 0);  /* open lib into global table */
  return 1;
}

