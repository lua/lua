/*
** LUA - Linguagem para Usuarios de Aplicacao
** Grupo de Tecnologia em Computacao Grafica
** TeCGraf - PUC-Rio
** $Id: lua.h,v 3.28 1996/05/06 14:32:59 roberto Exp $
*/


#ifndef lua_h
#define lua_h

#define LUA_VERSION	"Lua 2.4"
#define LUA_COPYRIGHT	"Copyright (C) 1994, 1995 TeCGraf"
#define LUA_AUTHORS 	"W. Celes, R. Ierusalimschy & L. H. de Figueiredo"


/* Private Part */
 
typedef enum
{
 LUA_T_NIL	= -1,
 LUA_T_NUMBER	= -2,
 LUA_T_STRING	= -3,
 LUA_T_ARRAY	= -4,
 LUA_T_FUNCTION	= -5,
 LUA_T_CFUNCTION= -6,
 LUA_T_MARK	= -7,
 LUA_T_CMARK	= -8,
 LUA_T_LINE	= -9,
 LUA_T_USERDATA = 0
} lua_Type;
 

/* Public Part */

#define LUA_NOOBJECT  0

typedef void (*lua_CFunction) (void);
typedef unsigned int lua_Object;

lua_Object     lua_setfallback		(char *name, lua_CFunction fallback);

void           lua_error		(char *s);
int            lua_dofile 		(char *filename);
int            lua_dostring 		(char *string);
int            lua_callfunction		(lua_Object function);
int	       lua_call			(char *funcname);

void	       lua_beginblock		(void);
void	       lua_endblock		(void);

lua_Object     lua_getparam 		(int number);
#define	       lua_getresult(_)		lua_getparam(_)

#define        lua_isnil(_)             (lua_type(_)==LUA_T_NIL)
#define        lua_istable(_)           (lua_type(_)==LUA_T_ARRAY)
#define        lua_isuserdata(_)        (lua_type(_)>=LUA_T_USERDATA)
#define        lua_iscfunction(_)       (lua_type(_)==LUA_T_CFUNCTION)
int            lua_isnumber             (lua_Object object);
int            lua_isstring             (lua_Object object);
int            lua_isfunction           (lua_Object object);

float          lua_getnumber 		(lua_Object object);
char          *lua_getstring 		(lua_Object object);
lua_CFunction  lua_getcfunction 	(lua_Object object);
void          *lua_getuserdata  	(lua_Object object);

void 	       lua_pushnil 		(void);
void           lua_pushnumber 		(float n);
void           lua_pushstring 		(char *s);
void           lua_pushcfunction	(lua_CFunction fn);
void           lua_pushusertag     	(void *u, int tag);
void           lua_pushobject       	(lua_Object object);

lua_Object     lua_getglobal 		(char *name);
void           lua_storeglobal		(char *name);

void           lua_storesubscript	(void);
lua_Object     lua_getsubscript         (void);

int            lua_type 		(lua_Object object);


int            lua_ref			(int lock);
lua_Object     lua_getref		(int ref);
void	       lua_pushref		(int ref);
void	       lua_unref		(int ref);

lua_Object     lua_createtable		(void);


/* some useful macros */

#define lua_refobject(o,l)	(lua_pushobject(o), lua_ref(l))

#define lua_register(n,f)	(lua_pushcfunction(f), lua_storeglobal(n))

#define lua_pushuserdata(u)     lua_pushusertag(u, LUA_T_USERDATA)


/* for compatibility with old versions. Avoid using these macros */

#define lua_lockobject(o)  lua_refobject(o,1)
#define	lua_lock() lua_ref(1)
#define lua_getlocked lua_getref
#define	lua_pushlocked lua_pushref
#define	lua_unlock lua_unref

#define lua_pushliteral(o)  lua_pushstring(o)

#define lua_getindexed(o,n) (lua_pushobject(o), lua_pushnumber(n), lua_getsubscript())
#define lua_getfield(o,f)   (lua_pushobject(o), lua_pushliteral(f), lua_getsubscript())

#define lua_copystring(o) (strdup(lua_getstring(o)))

#endif
