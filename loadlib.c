/*
** $Id: loadlib.c,v 1.8 2004/10/18 18:07:31 roberto Exp roberto $
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

LUALIB_API int luaopen_loadlib (lua_State *L)
{
 luaL_newmetatable(L, "_LOADLIB");
 lua_pushcfunction(L, gctm);
 lua_setfield(L, -2, "__gc");
 lua_register(L,"loadlib",loadlib1);
 return 0;
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
