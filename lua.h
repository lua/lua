/*
** LUA - Linguagem para Usuarios de Aplicacao
** Grupo de Tecnologia em Computacao Grafica
** TeCGraf - PUC-Rio
** $Id: lua.h,v 3.1 1994/11/02 20:30:53 roberto Exp roberto $
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
} Type;
 

/* Public Part */

typedef void (*lua_CFunction) (void);
typedef struct Object *lua_Object;

#define lua_register(n,f)	(lua_pushcfunction(f), lua_storeglobal(n))

void           lua_errorfunction    	(void (*fn) (char *s));
void           lua_error		(char *s);
int            lua_dofile 		(char *filename);
int            lua_dostring 		(char *string);
int            lua_callfunction		(lua_Object function);

lua_Object     lua_getparam 		(int number);
float          lua_getnumber 		(lua_Object object);
char          *lua_getstring 		(lua_Object object);
char 	      *lua_copystring 		(lua_Object object);
lua_CFunction  lua_getcfunction 	(lua_Object object);
void          *lua_getuserdata  	(lua_Object object);
void          *lua_gettable  	        (lua_Object object);
lua_Object     lua_getfield         	(lua_Object object, char *field);
lua_Object     lua_getindexed         	(lua_Object object, float index);
lua_Object     lua_getglobal 		(char *name);

int 	       lua_pushnil 		(void);
int            lua_pushnumber 		(float n);
int            lua_pushstring 		(char *s);
int            lua_pushcfunction	(lua_CFunction fn);
int            lua_pushuserdata     	(void *u);
int            lua_pushtable     	(void *t);
int            lua_pushsubscript	(void);
int            lua_pushobject       	(lua_Object object);

int            lua_storeglobal		(char *name);
int            lua_storefield 		(lua_Object object, char *field);
int            lua_storeindexed 	(lua_Object object, float index);
int            lua_storesubscript	(void);

int            lua_type 		(lua_Object object);


/* for lua 1.1 */

#define lua_call(f)           lua_callfunction(lua_getglobal(f))

#define         lua_getindexed(o,n)     (lua_pushnumber(n), lua_getIndex(o))
#define         lua_getfield(o,f)       (lua_pushstring(f), lua_getIndex(o))

#define lua_isnil(_)            (lua_type(_)==LUA_T_NIL)
#define lua_isnumber(_)         (lua_type(_)==LUA_T_NUMBER)
#define lua_isstring(_)         (lua_type(_)==LUA_T_STRING)
#define lua_istable(_)          (lua_type(_)==LUA_T_ARRAY)
#define lua_isfunction(_)       (lua_type(_)==LUA_T_FUNCTION)
#define lua_iscfunction(_)      (lua_type(_)==LUA_T_CFUNCTION)
#define lua_isuserdata(_)       (lua_type(_)>=LUA_T_USERDATA)

#endif
