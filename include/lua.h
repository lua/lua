/*
** LUA - Linguagem para Usuarios de Aplicacao
** Grupo de Tecnologia em Computacao Grafica
** TeCGraf - PUC-Rio
** $Id: lua.h,v 3.16 1995/01/27 17:19:06 celes Exp $
*/


#ifndef lua_h
#define lua_h

/* Private Part */
 
typedef enum
{
 LUA_T_NIL	= -1,
 LUA_T_NUMBER	= -2,
 LUA_T_STRING	= -3,
 LUA_T_ARRAY	= -4,
 LUA_T_FUNCTION	= -5,
 LUA_T_CFUNCTION= -6,
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

float          lua_getnumber 		(lua_Object object);
char          *lua_getstring 		(lua_Object object);
lua_CFunction  lua_getcfunction 	(lua_Object object);
void          *lua_getuserdata  	(lua_Object object);

void 	       lua_pushnil 		(void);
void           lua_pushnumber 		(float n);
void           lua_pushstring 		(char *s);
void           lua_pushliteral 		(char *s);
void           lua_pushcfunction	(lua_CFunction fn);
void           lua_pushusertag     	(void *u, int tag);
void           lua_pushobject       	(lua_Object object);

lua_Object     lua_getglobal 		(char *name);
void           lua_storeglobal		(char *name);

void           lua_storesubscript	(void);
lua_Object     lua_getsubscript         (void);

int            lua_type 		(lua_Object object);

int	       lua_lock			(void);
lua_Object     lua_getlocked		(int ref);
void	       lua_pushlocked		(int ref);
void	       lua_unlock		(int ref);

lua_Object     lua_createtable		(void);


/* some useful macros */

#define lua_lockobject(o)  (lua_pushobject(o), lua_lock())

#define lua_register(n,f)	(lua_pushcfunction(f), lua_storeglobal(n))

#define lua_pushuserdata(u)     lua_pushusertag(u, LUA_T_USERDATA)

#define lua_isnil(_)            (lua_type(_)==LUA_T_NIL)
#define lua_isnumber(_)         (lua_type(_)==LUA_T_NUMBER)
#define lua_isstring(_)         (lua_type(_)==LUA_T_STRING)
#define lua_istable(_)          (lua_type(_)==LUA_T_ARRAY)
#define lua_isfunction(_)       (lua_type(_)==LUA_T_FUNCTION)
#define lua_iscfunction(_)      (lua_type(_)==LUA_T_CFUNCTION)
#define lua_isuserdata(_)       (lua_type(_)>=LUA_T_USERDATA)


/* for lua 1.1 compatibility. Avoid using these macros */

#define lua_getindexed(o,n) (lua_pushobject(o), lua_pushnumber(n), lua_getsubscript())
#define lua_getfield(o,f)   (lua_pushobject(o), lua_pushliteral(f), lua_getsubscript())

#define lua_copystring(o) (strdup(lua_getstring(o)))

#endif
