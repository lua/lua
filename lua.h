/*
** $Id: lua.h,v 1.41 1999/11/29 19:31:29 roberto Exp roberto $
** Lua - An Extensible Extension Language
** TeCGraf: Grupo de Tecnologia em Computacao Grafica, PUC-Rio, Brazil
** e-mail: lua@tecgraf.puc-rio.br
** www: http://www.tecgraf.puc-rio.br/lua/
** See Copyright Notice at the end of this file
*/


#ifndef lua_h
#define lua_h

#define LUA_VERSION	"Lua 3.x"
#define LUA_COPYRIGHT	"Copyright (C) 1994-1999 TeCGraf, PUC-Rio"
#define LUA_AUTHORS 	"W. Celes, R. Ierusalimschy & L. H. de Figueiredo"


#define LUA_NOREF	(-2)
#define LUA_REFNIL	(-1)

#define LUA_ANYTAG	(-1)

typedef struct lua_State lua_State;

typedef void (*lua_CFunction) ( /* lua_State *L */ );

typedef struct TObject *lua_Object;

#define LUA_NOOBJECT	((lua_Object)0)


lua_State     *lua_newstate (void);
void           lua_close (lua_State *L);

lua_Object     lua_settagmethod (lua_State *L, int tag, const char *event);
                                                       /* In: new method */
lua_Object     lua_gettagmethod (lua_State *L, int tag, const char *event);

int            lua_newtag (lua_State *L);
int            lua_copytagmethods (lua_State *L, int tagto, int tagfrom);
void           lua_settag (lua_State *L, int tag); /* In: object */

void           lua_error (lua_State *L, const char *s);
int            lua_dofile (lua_State *L, const char *filename);
                                                        /* Out: returns */
int            lua_dostring (lua_State *L, const char *str);
                                                        /* Out: returns */
int            lua_dobuffer (lua_State *L, const char *buff, int size,
                                         const char *name); /* Out: returns */
int            lua_callfunction (lua_State *L, lua_Object f);
					  /* In: parameters; Out: returns */

void	       lua_beginblock (lua_State *L);
void	       lua_endblock (lua_State *L);

lua_Object     lua_lua2C (lua_State *L, int number);
#define	       lua_getparam		lua_lua2C
#define	       lua_getresult		lua_lua2C

const char    *lua_type (lua_State *L, lua_Object obj);

int            lua_isnil (lua_State *L, lua_Object obj);
int            lua_istable (lua_State *L, lua_Object obj);
int            lua_isuserdata (lua_State *L, lua_Object obj);
int            lua_iscfunction (lua_State *L, lua_Object obj);
int            lua_isnumber (lua_State *L, lua_Object obj);
int            lua_isstring (lua_State *L, lua_Object obj);
int            lua_isfunction (lua_State *L, lua_Object obj);

int            lua_equal (lua_State *L, lua_Object o1, lua_Object o2);

double         lua_getnumber (lua_State *L, lua_Object obj);
const char    *lua_getstring (lua_State *L, lua_Object obj);
long           lua_strlen (lua_State *L, lua_Object obj);
lua_CFunction  lua_getcfunction (lua_State *L, lua_Object obj);
void	      *lua_getuserdata (lua_State *L, lua_Object obj);


void 	       lua_pushnil (lua_State *L);
void           lua_pushnumber (lua_State *L, double n);
void           lua_pushlstring (lua_State *L, const char *s, long len);
void           lua_pushstring (lua_State *L, const char *s);
void           lua_pushcclosure (lua_State *L, lua_CFunction fn, int n);
void           lua_pushusertag (lua_State *L, void *u, int tag);
void           lua_pushobject (lua_State *L, lua_Object obj);

lua_Object     lua_pop (lua_State *L);

lua_Object     lua_getglobal (lua_State *L, const char *name);
lua_Object     lua_rawgetglobal (lua_State *L, const char *name);
void           lua_setglobal (lua_State *L, const char *name); /* In: value */
void           lua_rawsetglobal (lua_State *L, const char *name);/* In: value */

void           lua_settable (lua_State *L); /* In: table, index, value */
void           lua_rawsettable (lua_State *L); /* In: table, index, value */
lua_Object     lua_gettable (lua_State *L); /* In: table, index */
lua_Object     lua_rawgettable (lua_State *L); /* In: table, index */

int            lua_tag (lua_State *L, lua_Object obj);

const char    *lua_nextvar (lua_State *L, const char *varname); /* Out: value */
int            lua_next (lua_State *L, lua_Object o, int i);
						/* Out: ref, value */ 

int            lua_ref (lua_State *L, int lock); /* In: value */
lua_Object     lua_getref (lua_State *L, int ref);
void	       lua_unref (lua_State *L, int ref);

lua_Object     lua_createtable (lua_State *L);

long	       lua_collectgarbage (lua_State *L, long limit);


lua_Object     lua_seterrormethod (lua_State *L);  /* In: new method */

lua_State     *lua_setstate (lua_State *st);


/* 
** ===============================================================
** some useful macros
** ===============================================================
*/

#ifdef LUA_REENTRANT

#define lua_call(L,name)	lua_callfunction(L, lua_getglobal(L, name))
#define lua_pushref(L,ref)	lua_pushobject(L, lua_getref(L, ref))
#define lua_refobject(L,o,l)	(lua_pushobject(L, o), lua_ref(L, l))
#define lua_register(L,n,f)	(lua_pushcfunction(L, f), lua_setglobal(L, n))
#define lua_pushuserdata(L,u)	lua_pushusertag(L, u, 0)
#define lua_pushcfunction(L,f)	lua_pushcclosure(L, f, 0)
#define lua_clonetag(L,t)	lua_copytagmethods(L, lua_newtag(L), (t))

#else

#define lua_call(name)		lua_callfunction(lua_getglobal(name))
#define lua_pushref(ref)	lua_pushobject(lua_getref(ref))
#define lua_refobject(o,l)	(lua_pushobject(o), lua_ref(l))
#define lua_register(n,f)	(lua_pushcfunction(f), lua_setglobal(n))
#define lua_pushuserdata(u)	lua_pushusertag(u, 0)
#define lua_pushcfunction(f)	lua_pushcclosure(f, 0)
#define lua_clonetag(t)		lua_copytagmethods(lua_newtag(), (t))

#endif



#ifndef LUA_REENTRANT
/* 
** ===============================================================
** Macros for single-state use
** ===============================================================
*/

extern lua_State *lua_state;

#define lua_open()		((void)(lua_state?0:(lua_state=lua_newstate())))

#define lua_close()		(lua_close)(lua_state)
#define lua_setstate(st)	(lua_setstate)(lua_state, st)
#define lua_settagmethod(tag,event)	(lua_settagmethod)(lua_state, tag,event)
#define lua_gettagmethod(tag,event)	(lua_gettagmethod)(lua_state, tag,event)
#define lua_newtag()		(lua_newtag)(lua_state)
#define lua_copytagmethods(tagto,tagfrom)	\
		(lua_copytagmethods)(lua_state, tagto,tagfrom)
#define lua_settag(tag)		(lua_settag)(lua_state, tag)
#define lua_error(s)		(lua_error)(lua_state, s)
#define lua_dofile(filename)	(lua_dofile)(lua_state, filename)
#define lua_dostring(str)	(lua_dostring)(lua_state, str)
#define lua_callfunction(f)	(lua_callfunction)(lua_state, f)
#define lua_beginblock()	(lua_beginblock)(lua_state)
#define lua_endblock()		(lua_endblock)(lua_state)
#define lua_lua2C(number)	(lua_lua2C)(lua_state, number)
#define lua_type(obj)		(lua_type)(lua_state, obj)
#define lua_isnil(obj)		(lua_isnil)(lua_state, obj)
#define lua_istable(obj)	(lua_istable)(lua_state, obj)
#define lua_isuserdata(obj)	(lua_isuserdata)(lua_state, obj)
#define lua_iscfunction(obj)	(lua_iscfunction)(lua_state, obj)
#define lua_isnumber(obj)	(lua_isnumber)(lua_state, obj)
#define lua_isstring(obj)	(lua_isstring)(lua_state, obj)
#define lua_isfunction(obj)	(lua_isfunction)(lua_state, obj)
#define lua_equal(o1,o2)	(lua_equal)(lua_state, o1,o2)
#define lua_getnumber(obj)	(lua_getnumber)(lua_state, obj)
#define lua_getstring(obj)	(lua_getstring)(lua_state, obj)
#define lua_strlen(obj)		(lua_strlen)(lua_state, obj)
#define lua_getcfunction(obj)	(lua_getcfunction)(lua_state, obj)
#define lua_getuserdata(obj)	(lua_getuserdata)(lua_state, obj)
#define lua_pushnil()		(lua_pushnil)(lua_state)
#define lua_pushnumber(n)	(lua_pushnumber)(lua_state, n)
#define lua_pushlstring(s,len)	(lua_pushlstring)(lua_state, s,len)
#define lua_pushstring(s)	(lua_pushstring)(lua_state, s)
#define lua_pushcclosure(fn,n)	(lua_pushcclosure)(lua_state, fn,n)
#define lua_pushusertag(u,tag)	(lua_pushusertag)(lua_state, u,tag)
#define lua_pushobject(obj)	(lua_pushobject)(lua_state, obj)
#define lua_pop()		(lua_pop)(lua_state)
#define lua_getglobal(name)	(lua_getglobal)(lua_state, name)
#define lua_rawgetglobal(name)	(lua_rawgetglobal)(lua_state, name)
#define lua_setglobal(name)	(lua_setglobal)(lua_state, name)
#define lua_rawsetglobal(name)	(lua_rawsetglobal)(lua_state, name)
#define lua_settable()		(lua_settable)(lua_state)
#define lua_rawsettable()	(lua_rawsettable)(lua_state)
#define lua_gettable()		(lua_gettable)(lua_state)
#define lua_rawgettable()	(lua_rawgettable)(lua_state)
#define lua_tag(obj)		(lua_tag)(lua_state, obj)
#define lua_nextvar(varname)	(lua_nextvar)(lua_state, varname)
#define lua_next(o,i)		(lua_next)(lua_state, o,i)
#define lua_ref(lock)		(lua_ref)(lua_state, lock)
#define lua_getref(ref)		(lua_getref)(lua_state, ref)
#define lua_unref(ref)		(lua_unref)(lua_state, ref)
#define lua_createtable()	(lua_createtable)(lua_state)
#define lua_collectgarbage(limit)	(lua_collectgarbage)(lua_state, limit)
#define lua_seterrormethod()	(lua_seterrormethod)(lua_state)

#endif


#endif



/******************************************************************************
* Copyright (c) 1994-1999 TeCGraf, PUC-Rio.  All rights reserved.
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
