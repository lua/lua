/*
** $Id: lua.h,v 1.79 2000/10/31 12:44:07 roberto Exp $
** Lua - An Extensible Extension Language
** TeCGraf: Grupo de Tecnologia em Computacao Grafica, PUC-Rio, Brazil
** e-mail: lua@tecgraf.puc-rio.br
** www: http://www.tecgraf.puc-rio.br/lua/
** See Copyright Notice at the end of this file
*/


#ifndef lua_h
#define lua_h


/* definition of `size_t' */
#include <stddef.h>


/* mark for all API functions */
#ifndef LUA_API
#define LUA_API		extern
#endif


#define LUA_VERSION	"Lua 4.0"
#define LUA_COPYRIGHT	"Copyright (C) 1994-2000 TeCGraf, PUC-Rio"
#define LUA_AUTHORS 	"W. Celes, R. Ierusalimschy & L. H. de Figueiredo"


/* name of global variable with error handler */
#define LUA_ERRORMESSAGE	"_ERRORMESSAGE"


/* pre-defined references */
#define LUA_NOREF	(-2)
#define LUA_REFNIL	(-1)
#define LUA_REFREGISTRY	0

/* pre-defined tags */
#define LUA_ANYTAG	(-1)
#define LUA_NOTAG	(-2)


/* option for multiple returns in lua_call */
#define LUA_MULTRET	(-1)


/* minimum stack available for a C function */
#define LUA_MINSTACK	20


/* error codes for lua_do* */
#define LUA_ERRRUN	1
#define LUA_ERRFILE	2
#define LUA_ERRSYNTAX	3
#define LUA_ERRMEM	4
#define LUA_ERRERR	5


typedef struct lua_State lua_State;

typedef int (*lua_CFunction) (lua_State *L);

/*
** types returned by `lua_type'
*/
#define LUA_TNONE	(-1)

#define LUA_TUSERDATA	0
#define LUA_TNIL	1
#define LUA_TNUMBER	2
#define LUA_TSTRING	3
#define LUA_TTABLE	4
#define LUA_TFUNCTION	5



/*
** state manipulation
*/
LUA_API lua_State *lua_open (int stacksize);
LUA_API void       lua_close (lua_State *L);


/*
** basic stack manipulation
*/
LUA_API int   lua_gettop (lua_State *L);
LUA_API void  lua_settop (lua_State *L, int index);
LUA_API void  lua_pushvalue (lua_State *L, int index);
LUA_API void  lua_remove (lua_State *L, int index);
LUA_API void  lua_insert (lua_State *L, int index);
LUA_API int   lua_stackspace (lua_State *L);


/*
** access functions (stack -> C)
*/

LUA_API int            lua_type (lua_State *L, int index);
LUA_API const char    *lua_typename (lua_State *L, int t);
LUA_API int            lua_isnumber (lua_State *L, int index);
LUA_API int            lua_isstring (lua_State *L, int index);
LUA_API int            lua_iscfunction (lua_State *L, int index);
LUA_API int            lua_tag (lua_State *L, int index);

LUA_API int            lua_equal (lua_State *L, int index1, int index2);
LUA_API int            lua_lessthan (lua_State *L, int index1, int index2);

LUA_API double         lua_tonumber (lua_State *L, int index);
LUA_API const char    *lua_tostring (lua_State *L, int index);
LUA_API size_t         lua_strlen (lua_State *L, int index);
LUA_API lua_CFunction  lua_tocfunction (lua_State *L, int index);
LUA_API void	      *lua_touserdata (lua_State *L, int index);
LUA_API const void    *lua_topointer (lua_State *L, int index);


/*
** push functions (C -> stack)
*/
LUA_API void  lua_pushnil (lua_State *L);
LUA_API void  lua_pushnumber (lua_State *L, double n);
LUA_API void  lua_pushlstring (lua_State *L, const char *s, size_t len);
LUA_API void  lua_pushstring (lua_State *L, const char *s);
LUA_API void  lua_pushcclosure (lua_State *L, lua_CFunction fn, int n);
LUA_API void  lua_pushusertag (lua_State *L, void *u, int tag);


/*
** get functions (Lua -> stack)
*/
LUA_API void  lua_getglobal (lua_State *L, const char *name);
LUA_API void  lua_gettable (lua_State *L, int index);
LUA_API void  lua_rawget (lua_State *L, int index);
LUA_API void  lua_rawgeti (lua_State *L, int index, int n);
LUA_API void  lua_getglobals (lua_State *L);
LUA_API void  lua_gettagmethod (lua_State *L, int tag, const char *event);
LUA_API int   lua_getref (lua_State *L, int ref);
LUA_API void  lua_newtable (lua_State *L);


/*
** set functions (stack -> Lua)
*/
LUA_API void  lua_setglobal (lua_State *L, const char *name);
LUA_API void  lua_settable (lua_State *L, int index);
LUA_API void  lua_rawset (lua_State *L, int index);
LUA_API void  lua_rawseti (lua_State *L, int index, int n);
LUA_API void  lua_setglobals (lua_State *L);
LUA_API void  lua_settagmethod (lua_State *L, int tag, const char *event);
LUA_API int   lua_ref (lua_State *L, int lock);


/*
** "do" functions (run Lua code)
*/
LUA_API int   lua_call (lua_State *L, int nargs, int nresults);
LUA_API void  lua_rawcall (lua_State *L, int nargs, int nresults);
LUA_API int   lua_dofile (lua_State *L, const char *filename);
LUA_API int   lua_dostring (lua_State *L, const char *str);
LUA_API int   lua_dobuffer (lua_State *L, const char *buff, size_t size, const char *name);

/*
** Garbage-collection functions
*/
LUA_API int   lua_getgcthreshold (lua_State *L);
LUA_API int   lua_getgccount (lua_State *L);
LUA_API void  lua_setgcthreshold (lua_State *L, int newthreshold);

/*
** miscellaneous functions
*/
LUA_API int   lua_newtag (lua_State *L);
LUA_API int   lua_copytagmethods (lua_State *L, int tagto, int tagfrom);
LUA_API void  lua_settag (lua_State *L, int tag);

LUA_API void  lua_error (lua_State *L, const char *s);

LUA_API void  lua_unref (lua_State *L, int ref);

LUA_API int   lua_next (lua_State *L, int index);
LUA_API int   lua_getn (lua_State *L, int index);

LUA_API void  lua_concat (lua_State *L, int n);

LUA_API void *lua_newuserdata (lua_State *L, size_t size);


/* 
** ===============================================================
** some useful macros
** ===============================================================
*/

#define lua_pop(L,n)		lua_settop(L, -(n)-1)

#define lua_register(L,n,f)	(lua_pushcfunction(L, f), lua_setglobal(L, n))
#define lua_pushuserdata(L,u)	lua_pushusertag(L, u, 0)
#define lua_pushcfunction(L,f)	lua_pushcclosure(L, f, 0)
#define lua_clonetag(L,t)	lua_copytagmethods(L, lua_newtag(L), (t))

#define lua_isfunction(L,n)	(lua_type(L,n) == LUA_TFUNCTION)
#define lua_istable(L,n)	(lua_type(L,n) == LUA_TTABLE)
#define lua_isuserdata(L,n)	(lua_type(L,n) == LUA_TUSERDATA)
#define lua_isnil(L,n)		(lua_type(L,n) == LUA_TNIL)
#define lua_isnull(L,n)		(lua_type(L,n) == LUA_TNONE)

#define lua_getregistry(L)	lua_getref(L, LUA_REFREGISTRY)

#endif



/******************************************************************************
* Copyright (C) 1994-2000 TeCGraf, PUC-Rio.  All rights reserved.
* 
* Permission is hereby granted, without written agreement and without license
* or royalty fees, to use, copy, modify, and distribute this software and its
* documentation for any purpose, including commercial applications, subject to
* the following conditions:
* 
*  - The above copyright notice and this permission notice shall appear in all
*    copies or substantial portions of this software.
* 
*  - The origin of this software must not be misrepresented; you must not
*    claim that you wrote the original software. If you use this software in a
*    product, an acknowledgment in the product documentation would be greatly
*    appreciated (but it is not required).
* 
*  - Altered source versions must be plainly marked as such, and must not be
*    misrepresented as being the original software.
*    
* The authors specifically disclaim any warranties, including, but not limited
* to, the implied warranties of merchantability and fitness for a particular
* purpose.  The software provided hereunder is on an "as is" basis, and the
* authors have no obligation to provide maintenance, support, updates,
* enhancements, or modifications.  In no event shall TeCGraf, PUC-Rio, or the
* authors be held liable to any party for direct, indirect, special,
* incidental, or consequential damages arising out of the use of this software
* and its documentation.
* 
* The Lua language and this implementation have been entirely designed and
* written by Waldemar Celes Filho, Roberto Ierusalimschy and
* Luiz Henrique de Figueiredo at TeCGraf, PUC-Rio.
*
* This implementation contains no third-party code.
******************************************************************************/

