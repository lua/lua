/*
** LUA - Linguagem para Usuarios de Aplicacao
** Grupo de Tecnologia em Computacao Grafica
** TeCGraf - PUC-Rio
** $Id: lua.h,v 3.4 1994/11/07 18:27:39 roberto Exp roberto $
*/


#ifndef lua_h
#define lua_h

/* Private Part */
 
typedef enum
{
 LUA_T_MARK,
 LUA_T_NIL,
 LUA_T_NUMBER,
 LUA_T_STRING,
 LUA_T_ARRAY,
 LUA_T_FUNCTION,
 LUA_T_CFUNCTION,
 LUA_T_USERDATA
} lua_Type;
 

/* Public Part */

typedef void (*lua_CFunction) (void);
typedef unsigned int lua_Object;

lua_Object     lua_setfallback		(char *name, lua_CFunction fallback);

void           lua_error		(char *s);
int            lua_dofile 		(char *filename);
int            lua_dostring 		(char *string);
int            lua_callfunction		(lua_Object function);

lua_Object     lua_getparam 		(int number);
#define	       lua_getresult		lua_getparam

float          lua_getnumber 		(lua_Object object);
char          *lua_getstring 		(lua_Object object);
char 	      *lua_copystring 		(lua_Object object);
lua_CFunction  lua_getcfunction 	(lua_Object object);
void          *lua_getuserdata  	(lua_Object object);

int 	       lua_pushnil 		(void);
int            lua_pushnumber 		(float n);
int            lua_pushstring 		(char *s);
int            lua_pushcfunction	(lua_CFunction fn);
int            lua_pushuserdata     	(void *u);
int            lua_pushobject       	(lua_Object object);

lua_Object     lua_getglobal 		(char *name);
int            lua_storeglobal		(char *name);

int            lua_storesubscript	(void);
lua_Object     lua_getsubscript         (void);

int            lua_type 		(lua_Object object);

int	       lua_lock			(lua_Object object);
lua_Object     lua_getlocked		(int ref);
void	       lua_unlock		(int ref);


/* for lua 1.1 */

#define lua_register(n,f)	(lua_pushcfunction(f), lua_storeglobal(n))

#define lua_call(f)           lua_callfunction(lua_getglobal(f))

#define lua_getindexed(o,n) (lua_pushobject(o), lua_pushnumber(n), lua_getsubscript())
#define lua_getfield(o,f)   (lua_pushobject(o), lua_pushstring(f), lua_getsubscript())

#define lua_isnil(_)            (lua_type(_)==LUA_T_NIL)
#define lua_isnumber(_)         (lua_type(_)==LUA_T_NUMBER)
#define lua_isstring(_)         (lua_type(_)==LUA_T_STRING)
#define lua_istable(_)          (lua_type(_)==LUA_T_ARRAY)
#define lua_isfunction(_)       (lua_type(_)==LUA_T_FUNCTION)
#define lua_iscfunction(_)      (lua_type(_)==LUA_T_CFUNCTION)
#define lua_isuserdata(_)       (lua_type(_)>=LUA_T_USERDATA)

#endif
