/*
** LUA - Linguagem para Usuarios de Aplicacao
** Grupo de Tecnologia em Computacao Grafica
** TeCGraf - PUC-Rio
** $Id: lua.h,v 1.1 1993/12/17 18:41:19 celes Exp $
*/


#ifndef lua_h
#define lua_h

typedef void (*lua_CFunction) (void);
typedef struct Object *lua_Object;

#define lua_register(n,f)	(lua_pushcfunction(f), lua_storeglobal(n))


void           lua_errorfunction    	(void (*fn) (char *s));
void           lua_error		(char *s);
int            lua_dofile 		(char *filename);
int            lua_dostring 		(char *string);
int            lua_call 		(char *functionname, int nparam);

lua_Object     lua_getparam 		(int number);
float          lua_getnumber 		(lua_Object object);
char          *lua_getstring 		(lua_Object object);
char 	      *lua_copystring 		(lua_Object object);
lua_CFunction  lua_getcfunction 	(lua_Object object);
void          *lua_getuserdata  	(lua_Object object);
lua_Object     lua_getfield         	(lua_Object object, char *field);
lua_Object     lua_getindexed         	(lua_Object object, float index);
lua_Object     lua_getglobal 		(char *name);

lua_Object     lua_pop 			(void);

int 	       lua_pushnil 		(void);
int            lua_pushnumber 		(float n);
int            lua_pushstring 		(char *s);
int            lua_pushcfunction	(lua_CFunction fn);
int            lua_pushuserdata     	(void *u);
int            lua_pushobject       	(lua_Object object);

int            lua_storeglobal		(char *name);
int            lua_storefield		(lua_Object object, char *field);
int            lua_storeindexed		(lua_Object object, float index);

int            lua_isnil 		(lua_Object object);
int            lua_isnumber 		(lua_Object object);
int            lua_isstring 		(lua_Object object);
int            lua_istable          	(lua_Object object);
int            lua_iscfunction 		(lua_Object object);
int            lua_isuserdata 		(lua_Object object);

#endif
