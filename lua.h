/*
** $Id: lua.h,v 1.12 1997/12/18 18:32:39 roberto Exp roberto $
** Lua - An Extensible Extension Language
** TeCGraf: Grupo de Tecnologia em Computacao Grafica, PUC-Rio, Brazil
** e-mail: lua@tecgraf.puc-rio.br
** www: http://www.tecgraf.puc-rio.br/lua/
*/

/*********************************************************************
* Copyright  (c) 1994-1998 TeCGraf, PUC-Rio.
* Written by Waldemar Celes Filho, Roberto Ierusalimschy and
* Luiz Henrique de Figueiredo.
* All rights reserved.
* 
* Permission is hereby granted, without written agreement and with­
* out license or royalty fees, to use, copy, modify, and distribute
* this  software  and its documentation for any purpose, subject to
* the following conditions:
* 
* The above copyright notice and this permission notice  shall  ap­
* pear in all copies or substantial portions of this software.
* 
* The name "Lua" cannot be used for any modified form of this soft­
* ware that does not originate from the authors. Nevertheless,  the
* name  "Lua"  may and should be used to designate the language im­
* plemented and described in this package, even if embedded in  any
* other  system,  as  long  as  its syntax and semantics remain un­
* changed.
* 
* The authors specifically disclaim any warranties, including,  but
* not  limited  to,  the  implied warranties of merchantability and
* fitness for a particular purpose. The software provided hereunder
* is  on  an  "as  is" basis, and the authors have no obligation to
* provide maintenance, support, updates, enhancements, or modifica­
* tions.  In no event shall TeCGraf, PUC-Rio, or the authors be li­
* able to any party for direct, indirect, special,  incidental,  or
* consequential damages arising out of the use of this software and
* its documentation.
*********************************************************************/



#ifndef lua_h
#define lua_h

#define LUA_VERSION	"Lua 3.1 (alpha)"
#define LUA_COPYRIGHT	"Copyright (C) 1994-1998 TeCGraf"
#define LUA_AUTHORS 	"W. Celes, R. Ierusalimschy & L. H. de Figueiredo"


#define LUA_NOOBJECT  0

#define LUA_ANYTAG    (-1)

typedef void (*lua_CFunction) (void);
typedef unsigned int lua_Object;


void	       lua_open			(void);
void           lua_close		(void);

lua_Object     lua_settagmethod	(int tag, char *event); /* In: new method */
lua_Object     lua_gettagmethod	(int tag, char *event);
lua_Object     lua_seterrormethod (void);  /* In: new method */

int            lua_newtag		(void);
int            lua_copytagmethods	(int tagto, int tagfrom);
void           lua_settag		(int tag); /* In: object */

void           lua_error		(char *s);
int            lua_dofile 		(char *filename); /* Out: returns */
int            lua_dostring 		(char *string); /* Out: returns */
int            lua_callfunction		(lua_Object f);
					  /* In: parameters; Out: returns */

void	       lua_beginblock		(void);
void	       lua_endblock		(void);

lua_Object     lua_lua2C 		(int number);
lua_Object     lua_upvalue		(int n);
#define	       lua_getparam(_)		lua_lua2C(_)
#define	       lua_getresult(_)		lua_lua2C(_)

int            lua_isnil                (lua_Object object);
int            lua_istable              (lua_Object object);
int            lua_isuserdata           (lua_Object object);
int            lua_iscfunction          (lua_Object object);
int            lua_isnumber             (lua_Object object);
int            lua_isstring             (lua_Object object);
int            lua_isfunction           (lua_Object object);

double         lua_getnumber 		(lua_Object object);
char          *lua_getstring 		(lua_Object object);
lua_CFunction  lua_getcfunction 	(lua_Object object);
void	      *lua_getuserdata		(lua_Object object);


void 	       lua_pushnil 		(void);
void           lua_pushnumber 		(double n);
void           lua_pushstring 		(char *s);
void           lua_pushCclosure		(lua_CFunction fn, int n);
void           lua_pushusertag          (void *u, int tag);
void           lua_pushobject       	(lua_Object object);

lua_Object     lua_pop			(void);

lua_Object     lua_getglobal 		(char *name);
lua_Object     lua_rawgetglobal		(char *name);
void           lua_setglobal		(char *name); /* In: value */
void           lua_rawsetglobal		(char *name); /* In: value */

void           lua_settable	(void); /* In: table, index, value */
void           lua_rawsettable	(void); /* In: table, index, value */
lua_Object     lua_gettable 		(void); /* In: table, index */
lua_Object     lua_rawgettable		(void); /* In: table, index */

int            lua_tag			(lua_Object object);


int            lua_ref			(int lock); /* In: value */
lua_Object     lua_getref		(int ref);
void	       lua_unref		(int ref);

lua_Object     lua_createtable		(void);

long	       lua_collectgarbage	(long limit);


/* =============================================================== */
/* some useful macros */

#define lua_call(name)		lua_callfunction(lua_getglobal(name))

#define lua_pushref(ref)	lua_pushobject(lua_getref(ref))

#define lua_refobject(o,l)	(lua_pushobject(o), lua_ref(l))

#define lua_register(n,f)	(lua_pushcfunction(f), lua_setglobal(n))

#define lua_pushuserdata(u)     lua_pushusertag(u, 0)

#define lua_pushcfunction(f)	lua_pushCclosure(f, 0)

#define lua_clonetag(t)		lua_copytagmethods(lua_newtag(), (t))


/* ==========================================================================
** for compatibility with old versions. Avoid using these macros/functions
** If your program does need any of these, define LUA_COMPAT2_5
*/


#ifdef LUA_COMPAT2_5


lua_Object     lua_setfallback		(char *event, lua_CFunction fallback);

#define lua_storeglobal		lua_setglobal
#define lua_type		lua_tag

#define lua_lockobject(o)  lua_refobject(o,1)
#define	lua_lock() lua_ref(1)
#define lua_getlocked lua_getref
#define	lua_pushlocked lua_pushref
#define	lua_unlock lua_unref

#define lua_pushliteral(o)  lua_pushstring(o)

#define lua_getindexed(o,n) (lua_pushobject(o), lua_pushnumber(n), lua_gettable())
#define lua_getfield(o,f)   (lua_pushobject(o), lua_pushstring(f), lua_gettable())

#define lua_copystring(o) (strdup(lua_getstring(o)))

#define lua_getsubscript  lua_gettable
#define lua_storesubscript  lua_settable

#endif

#endif
