/*
** $Id: lua.h,v 1.147 2002/07/17 16:25:13 roberto Exp roberto $
** Lua - An Extensible Extension Language
** Tecgraf: Computer Graphics Technology Group, PUC-Rio, Brazil
** http://www.lua.org	mailto:info@lua.org
** See Copyright Notice at the end of this file
*/


#ifndef lua_h
#define lua_h


/* definition of `va_list' */
#include <stdarg.h>

/* definition of `size_t' */
#include <stddef.h>



#define LUA_VERSION	"Lua 5.0 (alpha)"
#define LUA_COPYRIGHT	"Copyright (C) 1994-2002 Tecgraf, PUC-Rio"
#define LUA_AUTHORS 	"W. Celes, R. Ierusalimschy & L. H. de Figueiredo"



/* option for multiple returns in `lua_pcall' and `lua_call' */
#define LUA_MULTRET	(-1)


/*
** pseudo-indices
*/
#define LUA_REGISTRYINDEX	(-10000)
#define LUA_GLOBALSINDEX	(-10001)
#define lua_upvalueindex(i)	(LUA_GLOBALSINDEX-(i))


/* error codes for `lua_load' and `lua_pcall' */
#define LUA_ERRRUN	1
#define LUA_ERRFILE	2
#define LUA_ERRSYNTAX	3
#define LUA_ERRMEM	4
#define LUA_ERRERR	5
#define LUA_ERRTHROW	6


typedef struct lua_State lua_State;

typedef int (*lua_CFunction) (lua_State *L);


/*
** functions that read blocks when loading Lua chunk
*/
typedef const char * (*lua_Chunkreader) (void *ud, size_t *size);


/*
** basic types
*/
#define LUA_TNONE	(-1)

#define LUA_TNIL	0
#define LUA_TNUMBER	1
#define LUA_TSTRING	2
#define LUA_TBOOLEAN	3
#define LUA_TTABLE	4
#define LUA_TFUNCTION	5
#define LUA_TUSERDATA	6
#define LUA_TLIGHTUSERDATA	7


/* minimum Lua stack available to a C function */
#define LUA_MINSTACK	20


/*
** generic extra include file
*/
#ifdef LUA_USER_H
#include LUA_USER_H
#endif


/* type of Numbers in Lua */
#ifndef LUA_NUMBER
#define LUA_NUMBER	double
#endif
typedef LUA_NUMBER lua_Number;


/* mark for all API functions */
#ifndef LUA_API
#define LUA_API		extern
#endif


/*
** state manipulation
*/
LUA_API lua_State *lua_open (void);
LUA_API void       lua_close (lua_State *L);
LUA_API lua_State *lua_newthread (lua_State *L);
LUA_API void       lua_closethread (lua_State *L, lua_State *thread);

LUA_API lua_CFunction lua_atpanic (lua_State *L, lua_CFunction panicf);


/*
** basic stack manipulation
*/
LUA_API int   lua_gettop (lua_State *L);
LUA_API void  lua_settop (lua_State *L, int index);
LUA_API void  lua_pushvalue (lua_State *L, int index);
LUA_API void  lua_remove (lua_State *L, int index);
LUA_API void  lua_insert (lua_State *L, int index);
LUA_API void  lua_replace (lua_State *L, int index);
LUA_API int   lua_checkstack (lua_State *L, int size);


/*
** access functions (stack -> C)
*/

LUA_API int             lua_isnumber (lua_State *L, int index);
LUA_API int             lua_isstring (lua_State *L, int index);
LUA_API int             lua_iscfunction (lua_State *L, int index);
LUA_API int             lua_type (lua_State *L, int index);
LUA_API const char     *lua_typename (lua_State *L, int type);

LUA_API int            lua_equal (lua_State *L, int index1, int index2);
LUA_API int            lua_rawequal (lua_State *L, int index1, int index2);
LUA_API int            lua_lessthan (lua_State *L, int index1, int index2);

LUA_API lua_Number      lua_tonumber (lua_State *L, int index);
LUA_API int             lua_toboolean (lua_State *L, int index);
LUA_API const char     *lua_tostring (lua_State *L, int index);
LUA_API size_t          lua_strlen (lua_State *L, int index);
LUA_API lua_CFunction   lua_tocfunction (lua_State *L, int index);
LUA_API void	       *lua_touserdata (lua_State *L, int index);
LUA_API const void     *lua_topointer (lua_State *L, int index);


/*
** push functions (C -> stack)
*/
LUA_API void  lua_pushnil (lua_State *L);
LUA_API void  lua_pushnumber (lua_State *L, lua_Number n);
LUA_API void  lua_pushlstring (lua_State *L, const char *s, size_t len);
LUA_API void  lua_pushstring (lua_State *L, const char *s);
LUA_API const char *lua_pushvfstring (lua_State *L, const char *fmt,
                                                    va_list argp);
LUA_API const char *lua_pushfstring (lua_State *L, const char *fmt, ...);
LUA_API void  lua_pushcclosure (lua_State *L, lua_CFunction fn, int n);
LUA_API void  lua_pushboolean (lua_State *L, int b);
LUA_API void  lua_pushlightuserdata (lua_State *L, void *p);


/*
** get functions (Lua -> stack)
*/
LUA_API void  lua_gettable (lua_State *L, int index);
LUA_API void  lua_rawget (lua_State *L, int index);
LUA_API void  lua_rawgeti (lua_State *L, int index, int n);
LUA_API void  lua_newtable (lua_State *L);
LUA_API int   lua_getmetatable (lua_State *L, int objindex);
LUA_API void  lua_getglobals (lua_State *L, int level);


/*
** set functions (stack -> Lua)
*/
LUA_API void  lua_settable (lua_State *L, int index);
LUA_API void  lua_rawset (lua_State *L, int index);
LUA_API void  lua_rawseti (lua_State *L, int index, int n);
LUA_API int   lua_setmetatable (lua_State *L, int objindex);
LUA_API int   lua_setglobals (lua_State *L, int level);


/*
** `load' and `call' functions (load and run Lua code)
*/
LUA_API void  lua_call (lua_State *L, int nargs, int nresults);
LUA_API int   lua_pcall (lua_State *L, int nargs, int nresults);
LUA_API int   lua_load (lua_State *L, lua_Chunkreader reader, void *data,
                        const char *chunkname);
LUA_API void  lua_pcallreset (lua_State *L);


/*
** coroutine functions
*/
LUA_API void lua_cobegin (lua_State *L, int nargs);
LUA_API int  lua_yield (lua_State *L, int nresults);
LUA_API int  lua_resume (lua_State *L, lua_State *co);

/*
** Garbage-collection functions
*/
LUA_API int   lua_getgcthreshold (lua_State *L);
LUA_API int   lua_getgccount (lua_State *L);
LUA_API void  lua_setgcthreshold (lua_State *L, int newthreshold);

/*
** miscellaneous functions
*/

LUA_API int   lua_error (lua_State *L);

LUA_API int   lua_next (lua_State *L, int index);

LUA_API void  lua_concat (lua_State *L, int n);

LUA_API void *lua_newuserdata (lua_State *L, size_t size);



/* 
** ===============================================================
** some useful macros
** ===============================================================
*/

#define lua_boxpointer(L,u) \
	(*(void **)(lua_newuserdata(L, sizeof(void *))) = (u))

#define lua_unboxpointer(L,i)	(*(void **)(lua_touserdata(L, i)))

#define lua_pop(L,n)		lua_settop(L, -(n)-1)

#define lua_register(L,n,f) \
	(lua_pushstring(L, n), \
	 lua_pushcfunction(L, f), \
	 lua_settable(L, LUA_GLOBALSINDEX))

#define lua_pushcfunction(L,f)	lua_pushcclosure(L, f, 0)

#define lua_isfunction(L,n)	(lua_type(L,n) == LUA_TFUNCTION)
#define lua_istable(L,n)	(lua_type(L,n) == LUA_TTABLE)
#define lua_isuserdata(L,n)	(lua_type(L,n) >= LUA_TUSERDATA)
#define lua_islightuserdata(L,n)	(lua_type(L,n) == LUA_TLIGHTUSERDATA)
#define lua_isnil(L,n)		(lua_type(L,n) == LUA_TNIL)
#define lua_isboolean(L,n)	(lua_type(L,n) == LUA_TBOOLEAN)
#define lua_isnone(L,n)		(lua_type(L,n) == LUA_TNONE)
#define lua_isnoneornil(L, n)	(lua_type(L,n) <= 0)

#define lua_pushliteral(L, s)	\
	lua_pushlstring(L, "" s, (sizeof(s)/sizeof(char))-1)



/*
** compatibility macros and functions
*/


LUA_API int lua_pushupvalues (lua_State *L);

#define lua_getregistry(L)	lua_pushvalue(L, LUA_REGISTRYINDEX)
#define lua_setglobal(L,s)	\
   (lua_pushstring(L, s), lua_insert(L, -2), lua_settable(L, LUA_GLOBALSINDEX))

#define lua_getglobal(L,s)	\
		(lua_pushstring(L, s), lua_gettable(L, LUA_GLOBALSINDEX))

#define lua_isnull	lua_isnone


/* compatibility with ref system */

/* pre-defined references */
#define LUA_NOREF	(-2)
#define LUA_REFNIL	(-1)

#define lua_ref(L,lock)	((lock) ? luaL_ref(L, LUA_REGISTRYINDEX) : \
      (lua_pushstring(L, "unlocked references are obsolete"), lua_error(L), 0))

#define lua_unref(L,ref)	luaL_unref(L, LUA_REGISTRYINDEX, (ref))

#define lua_getref(L,ref)	lua_rawgeti(L, LUA_REGISTRYINDEX, ref)

#endif



/*
** {======================================================================
** useful definitions for Lua kernel and libraries
** =======================================================================
*/

/* formats for Lua numbers */
#ifndef LUA_NUMBER_SCAN
#define LUA_NUMBER_SCAN		"%lf"
#endif

#ifndef LUA_NUMBER_FMT
#define LUA_NUMBER_FMT		"%.16g"
#endif

/* }====================================================================== */



/******************************************************************************
* Copyright (C) 2002 Tecgraf, PUC-Rio.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************/

