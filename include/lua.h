/*
** LUA - An Extensible Extension Language
** TeCGraf: Grupo de Tecnologia em Computacao Grafica, PUC-Rio, Brazil
** e-mail: lua@tecgraf.puc-rio.br
** $Id: lua.h,v 4.11 1997/06/23 18:27:53 roberto Exp $
*/


#ifndef lua_h
#define lua_h

#define LUA_VERSION	"Lua 3.0"
#define LUA_COPYRIGHT	"Copyright (C) 1994-1997 TeCGraf"
#define LUA_AUTHORS 	"W. Celes, R. Ierusalimschy & L. H. de Figueiredo"


#define LUA_NOOBJECT  0

#define LUA_ANYTAG    (-1)

typedef void (*lua_CFunction) (void);
typedef unsigned int lua_Object;

lua_Object     lua_settagmethod	(int tag, char *event);  /* In: new method */
lua_Object     lua_gettagmethod	(int tag, char *event);
lua_Object     lua_seterrormethod (void);  /* In: new method */

int            lua_newtag		(void);
void           lua_settag		(int tag); /* In: object */

void           lua_error		(char *s);
int            lua_dofile 		(char *filename); /* Out: returns */
int            lua_dostring 		(char *string); /* Out: returns */
int            lua_callfunction		(lua_Object f);
					  /* In: parameters; Out: returns */

void	       lua_beginblock		(void);
void	       lua_endblock		(void);

lua_Object     lua_lua2C 		(int number);
#define	       lua_getparam(_)		lua_lua2C(_)
#define	       lua_getresult(_)		lua_lua2C(_)

int            lua_isnil                (lua_Object object);
int            lua_istable              (lua_Object object);
int            lua_isuserdata           (lua_Object object);
int            lua_iscfunction          (lua_Object object);
int            lua_isnumber             (lua_Object object);
int            lua_isstring             (lua_Object object);
int            lua_isfunction           (lua_Object object);

float          lua_getnumber 		(lua_Object object);
char          *lua_getstring 		(lua_Object object);
lua_CFunction  lua_getcfunction 	(lua_Object object);
void	      *lua_getuserdata		(lua_Object object);


void 	       lua_pushnil 		(void);
void           lua_pushnumber 		(float n);
void           lua_pushstring 		(char *s);
void           lua_pushcfunction	(lua_CFunction fn);
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




/* ========================================================================== 
** for compatibility with old versions. Avoid using these macros/functions
** If your program does not use any of these, define LUA_COMPAT2_5 to 0
*/

#ifndef LUA_COMPAT2_5
#define LUA_COMPAT2_5	1
#endif


#if LUA_COMPAT2_5


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
