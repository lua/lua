/*
** $Id: loadlib.c,v 1.4 2003/04/07 20:11:53 roberto Exp $
** Dynamic library loader for Lua
** See Copyright Notice in lua.h
*
* This  Lua library  exports a  single function,  called loadlib,  which is
* called from Lua  as loadlib(lib,init), where lib is the  full name of the
* library to be  loaded (including the complete path) and  init is the name
* of a function  to be called after the library  is loaded. Typically, this
* function will register other functions,  thus making the complete library
* available  to Lua.  The init  function is  *not* automatically  called by
* loadlib. Instead,  loadlib returns  the init function  as a  Lua function
* that the client  can call when it  thinks is appropriate. In  the case of
* errors,  loadlib  returns  nil  and two  strings  describing  the  error.
* The  first string  is  supplied by  the operating  system;  it should  be
* informative and useful  for error messages. The second  string is "open",
* "init", or  "absent" to identify  the error and is  meant to be  used for
* making  decisions without  having to  look into  the first  string (whose
* format is system-dependent).
*
* This module contains  an implementation of loadlib for  Unix systems that
* have dlfcn, an implementation for Windows,  and a stub for other systems.
* See  the list  at  the end  of  this  file for  some  links to  available
* implementations of dlfcn  and interfaces to other  native dynamic loaders
* on top of which loadlib could be implemented.
*
*/

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"


#undef LOADLIB


#ifdef USE_DLOPEN
#define LOADLIB
/*
* This is an implementation of loadlib based on the dlfcn interface.
* The dlfcn interface is available in Linux, SunOS, Solaris, IRIX, FreeBSD,
* NetBSD, AIX 4.2, HPUX 11, and  probably most other Unix flavors, at least
* as an emulation layer on top of native functions.
*/

#include <dlfcn.h>

static int loadlib(lua_State *L)
{
 const char *path=luaL_checkstring(L,1);
 const char *init=luaL_checkstring(L,2);
 void *lib=dlopen(path,RTLD_NOW);
 if (lib!=NULL)
 {
  lua_CFunction f=(lua_CFunction) dlsym(lib,init);
  if (f!=NULL)
  {
   lua_pushlightuserdata(L,lib);
   lua_pushcclosure(L,f,1);
   return 1;
  }
 }
 /* else return appropriate error messages */
 lua_pushnil(L);
 lua_pushstring(L,dlerror());
 lua_pushstring(L,(lib!=NULL) ? "init" : "open");
 if (lib!=NULL) dlclose(lib);
 return 3;
}

#endif



/*
** In Windows, default is to use dll; otherwise, default is not to use dll
*/
#ifndef USE_DLL
#ifdef _WIN32
#define USE_DLL	1
#else
#define USE_DLL	0
#endif
#endif


#if USE_DLL
#define LOADLIB
/*
* This is an implementation of loadlib for Windows using native functions.
*/

#include <windows.h>

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

static int loadlib(lua_State *L)
{
 const char *path=luaL_checkstring(L,1);
 const char *init=luaL_checkstring(L,2);
 HINSTANCE lib=LoadLibrary(path);
 if (lib!=NULL)
 {
  lua_CFunction f=(lua_CFunction) GetProcAddress(lib,init);
  if (f!=NULL)
  {
   lua_pushlightuserdata(L,lib);
   lua_pushcclosure(L,f,1);
   return 1;
  }
 }
 lua_pushnil(L);
 pusherror(L);
 lua_pushstring(L,(lib!=NULL) ? "init" : "open");
 if (lib!=NULL) FreeLibrary(lib);
 return 3;
}

#endif



#ifndef LOADLIB
/* Fallback for other systems */

/*
** Those systems support dlopen, so they should have defined USE_DLOPEN.
** The default (no)implementation gives them a special error message.
*/
#ifdef linux
#define LOADLIB
#endif

#ifdef sun
#define LOADLIB
#endif

#ifdef sgi
#define LOADLIB
#endif

#ifdef BSD
#define LOADLIB
#endif

#ifdef _WIN32
#define LOADLIB
#endif

#ifdef LOADLIB
#undef LOADLIB
#define LOADLIB	"`loadlib' not installed (check your Lua configuration)"
#else
#define LOADLIB	"`loadlib' not supported"
#endif

static int loadlib(lua_State *L)
{
 lua_pushnil(L);
 lua_pushliteral(L,LOADLIB);
 lua_pushliteral(L,"absent");
 return 3;
}
#endif

LUALIB_API int luaopen_loadlib (lua_State *L)
{
 lua_register(L,"loadlib",loadlib);
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
* Mac OS X/Darwin
* http://www.opendarwin.org/projects/dlcompat/
*
* GLIB has wrapper code for BeOS, OS2, Unix and Windows
* http://cvs.gnome.org/lxr/source/glib/gmodule/
*
*/
