/*
** LUA - Linguagem para Usuarios de Aplicacao
** Grupo de Tecnologia em Computacao Grafica
** TeCGraf - PUC-Rio
** $Id: lua.h,v 3.38 1997/03/31 20:59:09 roberto Exp roberto $
*/


#ifndef lua_h
#define lua_h

#define LUA_VERSION	"Lua 3.0 (alpha)"
#define LUA_COPYRIGHT	"Copyright (C) 1994-1997 TeCGraf"
#define LUA_AUTHORS 	"W. Celes, R. Ierusalimschy & L. H. de Figueiredo"


#define LUA_NOOBJECT  0

typedef void (*lua_CFunction) (void);
typedef unsigned int lua_Object;

lua_Object     lua_setfallback		(char *event, lua_CFunction fallback);
void           lua_setintmethod	(int tag, char *event, lua_CFunction method);
void           lua_seterrormethod (lua_CFunction method);

int            lua_newtag		(char *t);
void           lua_settag		(int tag); /* In: object */

void           lua_error		(char *s);
int            lua_dofile 		(char *filename); /* Out: returns */
int            lua_dostring 		(char *string); /* Out: returns */
int            lua_callfunction		(lua_Object f);
					  /* In: parameters; Out: returns */
int	       lua_call			(char *funcname);
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
void          *lua_getbinarydata	(lua_Object object);
int            lua_getbindatasize	(lua_Object object);

void 	       lua_pushnil 		(void);
void           lua_pushnumber 		(float n);
void           lua_pushstring 		(char *s);
void           lua_pushcfunction	(lua_CFunction fn);
void           lua_pushbinarydata	(void *buff, int size, int tag);
void           lua_pushusertag     	(void *u, int tag);
void           lua_pushobject       	(lua_Object object);

lua_Object     lua_getglobal 		(char *name);
lua_Object     lua_basicgetglobal	(char *name);
void           lua_setglobal		(char *name); /* In: value */
void           lua_basicsetglobal	(char *name); /* In: value */

void           lua_storesubscript	(void); /* In: table, index, value */
void           lua_basicstoreindex	(void); /* In: table, index, value */
lua_Object     lua_getsubscript		(void); /* In: table, index */
lua_Object     lua_basicindex		(void); /* In: table, index */

int            lua_tag			(lua_Object object);


int            lua_ref			(int lock); /* In: value */
lua_Object     lua_getref		(int ref);
void	       lua_pushref		(int ref);
void	       lua_unref		(int ref);

lua_Object     lua_createtable		(void);


/* =============================================================== */
/* some useful macros */

#define lua_refobject(o,l)	(lua_pushobject(o), lua_ref(l))

#define lua_register(n,f)	(lua_pushcfunction(f), lua_storeglobal(n))

#define lua_pushuserdata(u)     lua_pushusertag(u, 0)


/* =============================================================== */
/* for compatibility with old versions. Avoid using these macros */

#define lua_storeglobal(n)	lua_setglobal(n)
#define lua_type(o)		(lua_tag(o))

#define lua_getuserdata(o)      (*(void **)lua_getbinarydata(o))

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
