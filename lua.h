/*
** $Id: lua.h,v 1.61 2000/08/29 14:33:31 roberto Exp roberto $
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


#define LUA_VERSION	"Lua 4.0 (beta)"
#define LUA_COPYRIGHT	"Copyright (C) 1994-2000 TeCGraf, PUC-Rio"
#define LUA_AUTHORS 	"W. Celes, R. Ierusalimschy & L. H. de Figueiredo"


#define LUA_ALERT		"_ALERT"
#define LUA_ERRORMESSAGE	"_ERRORMESSAGE"


#define LUA_NOREF	(-2)
#define LUA_REFNIL	(-1)

#define LUA_ANYTAG	(-1)

#define LUA_MULTRET	(-1)


#define LUA_MINSTACK	16


/* error codes for lua_do* */
#define LUA_ERRFILE	2
#define LUA_ERRSYNTAX	3
#define LUA_ERRRUN	1
#define LUA_ERRMEM	4


typedef struct lua_State lua_State;

typedef int (*lua_CFunction) (lua_State *L);


/*
** state manipulation
*/
lua_State     *lua_newstate (int stacksize, int builtin);
void           lua_close (lua_State *L);


/*
** basic stack manipulation
*/
int            lua_gettop (lua_State *L);
void           lua_settop (lua_State *L, int index);
void           lua_pushobject (lua_State *L, int index);
int            lua_stackspace (lua_State *L);


/*
** access functions (stack -> C)
*/

const char    *lua_type (lua_State *L, int index);
int            lua_isnumber (lua_State *L, int index);
int            lua_iscfunction (lua_State *L, int index);
int            lua_tag (lua_State *L, int index);

int            lua_equal (lua_State *L, int index1, int index2);

double         lua_tonumber (lua_State *L, int index);
const char    *lua_tostring (lua_State *L, int index);
size_t         lua_strlen (lua_State *L, int index);
lua_CFunction  lua_tocfunction (lua_State *L, int index);
void	      *lua_touserdata (lua_State *L, int index);


/*
** push functions (C -> stack)
*/
void           lua_pushnil (lua_State *L);
void           lua_pushnumber (lua_State *L, double n);
void           lua_pushlstring (lua_State *L, const char *s, size_t len);
void           lua_pushstring (lua_State *L, const char *s);
void           lua_pushcclosure (lua_State *L, lua_CFunction fn, int n);
void           lua_pushusertag (lua_State *L, void *u, int tag);


/*
** get functions (Lua -> stack)
*/
void           lua_getglobal (lua_State *L, const char *name);
void           lua_gettable (lua_State *L);
void           lua_rawget (lua_State *L);
void           lua_getglobals (lua_State *L);
void           lua_gettagmethod (lua_State *L, int tag, const char *event);

int            lua_getref (lua_State *L, int ref);

void           lua_newtable (lua_State *L);


/*
** set functions (stack -> Lua)
*/
void           lua_setglobal (lua_State *L, const char *name);
void           lua_settable (lua_State *L);
void           lua_rawset (lua_State *L);
void           lua_setglobals (lua_State *L);
void           lua_settagmethod (lua_State *L, int tag, const char *event);
int            lua_ref (lua_State *L, int lock);


/*
** "do" functions (run Lua code)
*/
int            lua_call (lua_State *L, int nargs, int nresults);
int            lua_dofile (lua_State *L, const char *filename);
int            lua_dostring (lua_State *L, const char *str);
int            lua_dobuffer (lua_State *L, const char *buff, size_t size,
                             const char *name);


/*
** miscelaneous functions
*/
int            lua_newtag (lua_State *L);
int            lua_copytagmethods (lua_State *L, int tagto, int tagfrom);
void           lua_settag (lua_State *L, int tag);

void           lua_error (lua_State *L, const char *s);

void	       lua_unref (lua_State *L, int ref);

long	       lua_collectgarbage (lua_State *L, long limit);

int            lua_next (lua_State *L, int index, int i);



/* 
** ===============================================================
** some useful macros
** ===============================================================
*/

#define lua_register(L,n,f)	(lua_pushcfunction(L, f), lua_setglobal(L, n))
#define lua_pushuserdata(L,u)	lua_pushusertag(L, u, 0)
#define lua_pushcfunction(L,f)	lua_pushcclosure(L, f, 0)
#define lua_clonetag(L,t)	lua_copytagmethods(L, lua_newtag(L), (t))

#define lua_isfunction(L,n)	(*lua_type(L,n) == 'f')
#define lua_isstring(L,n)	(lua_tostring(L,n) != 0)
#define lua_istable(L,n)	(*lua_type(L,n) == 't')
#define lua_isuserdata(L,n)	(*lua_type(L,n) == 'u')
#define lua_isnil(L,n)		(lua_type(L,n)[2] == 'l')
#define lua_isnull(L,n)		(*lua_type(L,n) == 'N')

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
