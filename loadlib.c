/*
** $Id: loadlib.c,v 1.10 2004/11/18 19:53:49 roberto Exp roberto $
** Dynamic library loader for Lua
** See Copyright Notice in lua.h
*
* This Lua library exports a single function, called loadlib, which
* is called from Lua as loadlib(lib,init), where lib is the full
* name of the library to be loaded (including the complete path) and
* init is the name of a function to be called after the library is
* loaded. Typically, this function will register other functions, thus
* making the complete library available to Lua.  The init function is
* *not* automatically called by loadlib. Instead, loadlib returns the
* init function as a Lua function that the client can call when it
* thinks is appropriate. In the case of errors, loadlib returns nil and
* two strings describing the error.  The first string is supplied by
* the operating system; it should be informative and useful for error
* messages. The second string is "open", "init", or "absent" to identify
* the error and is meant to be used for making decisions without having
* to look into the first string (whose format is system-dependent).
* This module contains an implementation of loadlib for Unix systems
* that have dlfcn, an implementation for Darwin (Mac OS X), an
* implementation for Windows, and a stub for other systems.  See
* the list at the end of this file for some links to available
* implementations of dlfcn and interfaces to other native dynamic
* loaders on top of which loadlib could be implemented.
*/


#include <stdlib.h>
#include <string.h>


#define loadlib_c
#define LUA_LIB

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"


#define ERR_OPEN	1
#define ERR_FUNCTION	2


static void registerlib (lua_State *L, const void *lib);


#if defined(USE_DLOPEN)
/*
* This is an implementation of loadlib based on the dlfcn interface.
* The dlfcn interface is available in Linux, SunOS, Solaris, IRIX, FreeBSD,
* NetBSD, AIX 4.2, HPUX 11, and  probably most other Unix flavors, at least
* as an emulation layer on top of native functions.
*/

#include <dlfcn.h>

#define freelib		dlclose

static int loadlib(lua_State *L, const char *path, const char *init)
{
 void *lib=dlopen(path,RTLD_NOW);
 if (lib!=NULL)
 {
  lua_CFunction f=(lua_CFunction) dlsym(lib,init);
  if (f!=NULL)
  {
   registerlib(L, lib);
   lua_pushcfunction(L,f);
   return 0;
  }
 }
 /* else return appropriate error message */
 lua_pushstring(L,dlerror());
 if (lib!=NULL) {
   dlclose(lib);
   return ERR_FUNCTION;  /* error loading function */
 }
 else return ERR_OPEN;  /* error loading library */
}



#elif defined(USE_DLL)
/*
* This is an implementation of loadlib for Windows using native functions.
*/

#include <windows.h>

#define freelib(lib)	FreeLibrary((HINSTANCE)lib)

static void pusherror(lua_State *L)
{
 int error=GetLastError();
 char buffer[128];
 if (FormatMessage(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
	0, error, 0, buffer, sizeof(buffer), 0))
  lua_pushstring(L,buffer);
 else
  lua_pushfstring(L,"system error %d\n",error);
}

static int loadlib(lua_State *L, const char *path, const char *init)
{
 HINSTANCE lib=LoadLibrary(path);
 if (lib!=NULL)
 {
  lua_CFunction f=(lua_CFunction) GetProcAddress(lib,init);
  if (f!=NULL)
  {
   registerlib(L, lib);
   lua_pushcfunction(L,f);
   return 1;
  }
 }
 pusherror(L);
 if (lib!=NULL) {
  FreeLibrary(lib);
  return ERR_OPEN;
 }
 else return ERR_FUNCTION;
}



/* Native Mac OS X / Darwin Implementation */
#elif defined(USE_DYLD)

#include <mach-o/dyld.h>


/* Mach cannot unload images (?) */
#define freelib(lib)	((void)lib)

static void pusherror (lua_State *L)
{
 const char *err_str;
 const char *err_file;
 NSLinkEditErrors err;
 int err_num;
 NSLinkEditError(&err, &err_num, &err_file, &err_str);
 lua_pushstring(L, err_str);
}

static int loadlib (lua_State *L, const char *path, const char *init) {
  const struct mach_header *lib;
  /* this would be a rare case, but prevents crashing if it happens */
  if(!_dyld_present()) {
    lua_pushstring(L,"dyld not present.");
    return ERR_OPEN;
  }
  lib = NSAddImage(path, NSADDIMAGE_OPTION_RETURN_ON_ERROR);
  if(lib != NULL) {
    NSSymbol init_sym = NSLookupSymbolInImage(lib, init,
                              NSLOOKUPSYMBOLINIMAGE_OPTION_BIND |
                              NSLOOKUPSYMBOLINIMAGE_OPTION_RETURN_ON_ERROR);
    if(init_sym != NULL) {
      lua_CFunction f = (lua_CFunction)NSAddressOfSymbol(init_sym);
      registerlib(L, lib);
      lua_pushcfunction(L,f);
      return 0;
    }
  }
  /* else an error ocurred */
  pusherror(L);
  /* Can't unload image */
  return (lib != NULL) ? ERR_FUNCTION : ERR_OPEN;
}



#else
/* Fallback for other systems */


#define freelib(lib)	((void)lib)

static int loadlib(lua_State *L)
{
 registerlib(L, NULL);  /* to avoid warnings */
 lua_pushnil(L);
 lua_pushliteral(L,"`loadlib' not supported");
 lua_pushliteral(L,"absent");
 return 3;
}

#endif


/*
** common code for all implementations: put a library into the registry
** so that, when Lua closes its state, the library's __gc metamethod
** will be called to unload the library.
*/
static void registerlib (lua_State *L, const void *lib)
{
 const void **plib = (const void **)lua_newuserdata(L, sizeof(const void *));
 *plib = lib;
  luaL_getmetatable(L, "_LOADLIB");
  lua_setmetatable(L, -2);
  lua_pushboolean(L, 1);
  lua_settable(L, LUA_REGISTRYINDEX);  /* registry[lib] = true */
}

/*
** __gc tag method: calls library's `freelib' function with the lib
** handle
*/
static int gctm (lua_State *L)
{
 void *lib = *(void **)luaL_checkudata(L, 1, "_LOADLIB");
 freelib(lib);
 return 0;
}


static int loadlib1 (lua_State *L) {
 const char *path=luaL_checkstring(L,1);
 const char *init=luaL_checkstring(L,2);
 int res = loadlib(L,path,init);
 if (res == 0)  /* no error? */
   return 1;  /* function is at stack top */
 else {  /* error */
  lua_pushnil(L);
  lua_insert(L,-2);  /* insert nil before error message */
  lua_pushstring(L, (res==ERR_OPEN)?"open":"init");
  return 3;
 }
}



/*
** {======================================================
** `require' and `module' functions
** =======================================================
*/


static const char *loadLua (lua_State *L, const char *fname, const char *name) {
  const char *path;
  /* try first `LUA_PATH' for compatibility */
  lua_getglobal(L, "LUA_PATH");
  path = lua_tostring(L, -1);
  if (!path) {
    lua_pop(L, 1);
    luaL_getfield(L, LUA_GLOBALSINDEX, "package.path");
    path = lua_tostring(L, -1);
  }
  if (path == NULL)
    luaL_error(L, "`package.path' must be a string");
  fname = luaL_searchpath(L, fname, path);
  if (fname == NULL) return path;  /* library not found in this path */
  if (luaL_loadfile(L, fname) != 0)
    luaL_error(L, "error loading package `%s' (%s)", name, lua_tostring(L, -1));
  return NULL;  /* library loaded successfully */
}


static const char *loadC (lua_State *L, const char *fname, const char *name) {
  const char *path;
  const char *funcname;
  luaL_getfield(L, LUA_GLOBALSINDEX, "package.cpath");
  path = lua_tostring(L, -1);
  if (path == NULL)
    luaL_error(L, "global _CPATH must be a string");
  fname = luaL_searchpath(L, fname, path);
  if (fname == NULL) return path;  /* library not found in this path */
  funcname = luaL_gsub(L, name, ".", "");
  funcname = lua_pushfstring(L, "%s%s", LUA_POF, funcname);
  if (loadlib(L, fname, funcname) != 0)
    luaL_error(L, "error loading package `%s' (%s)", name, lua_tostring(L, -1));
  return NULL;  /* library loaded successfully */
}


static int ll_require (lua_State *L) {
  const char *name = luaL_checkstring(L, 1);
  lua_settop(L, 1);
  lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
  lua_getfield(L, 2, name);
  if (lua_toboolean(L, -1))  /* is it there? */
    return 1;  /* package is already loaded; return its result */
  /* else must load it; first mark it as loaded */
  lua_pushboolean(L, 1);
  lua_setfield(L, 2, name);  /* _LOADED[name] = true */
  /* check whether it is preloaded */
  lua_getfield(L, LUA_REGISTRYINDEX, "_PRELOAD");
  lua_getfield(L, -1, name);
  if (lua_isnil(L, -1)) {  /* no preload function for that module? */
    const char *fname = luaL_gsub(L, name, ".", LUA_DIRSEP);
    const char *cpath = loadC(L, fname, name);  /* try a C module */
    if (cpath) {  /* not found? */
      const char *path = loadLua(L, fname, name);  /* try a Lua module */
      if (path) {  /* yet not found? */
        lua_pushnil(L);
        lua_setfield(L, 2, name);  /* unmark _LOADED[name] */
        return luaL_error(L, "package `%s' not found\n"
                             "  cpath: %s\n  path: %s",
                             name, cpath, path);
      }
    }
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
  {"loadlib", loadlib1},
  {"require", ll_require},
  {"module", ll_module},
  {NULL, NULL}
};



LUALIB_API int luaopen_loadlib (lua_State *L) {
  const char *path;
  /* create new type _LOADLIB */
  luaL_newmetatable(L, "_LOADLIB");
  lua_pushcfunction(L, gctm);
  lua_setfield(L, -2, "__gc");
  /* create `package' table */
  lua_newtable(L);
  lua_pushvalue(L, -1);
  lua_setglobal(L, "package");
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
  lua_getfield(L, LUA_REGISTRYINDEX, "_PRELOAD");
  lua_setfield(L, -2, "preload");
  lua_pushvalue(L, LUA_GLOBALSINDEX);
  luaL_openlib(L, NULL, ll_funcs, 0);  /* open lib into global table */
  return 1;
}

/*
* Here are some links to available implementations of dlfcn and
* interfaces to other native dynamic loaders on top of which loadlib
* could be implemented. Please send contributions and corrections to us.
*
* AIX
* Starting with AIX 4.2, dlfcn is included in the base OS.
* There is also an emulation package available.
* http://www.faqs.org/faqs/aix-faq/part4/section-21.html
*
* HPUX 
* HPUX 11 has dlfcn. For HPUX 10 use shl_*.
* http://www.geda.seul.org/mailinglist/geda-dev37/msg00094.html
* http://www.stat.umn.edu/~luke/xls/projects/dlbasics/dlbasics.html
*
* Macintosh, Windows
* http://www.stat.umn.edu/~luke/xls/projects/dlbasics/dlbasics.html
*
* GLIB has wrapper code for BeOS, OS2, Unix and Windows
* http://cvs.gnome.org/lxr/source/glib/gmodule/
*
*/
