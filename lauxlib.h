/*
** $Id: lauxlib.h,v 1.14 1999/11/22 13:12:07 roberto Exp roberto $
** Auxiliary functions for building Lua libraries
** See Copyright Notice in lua.h
*/


#ifndef auxlib_h
#define auxlib_h


#include "lua.h"


struct luaL_reg {
  const char *name;
  lua_CFunction func;
};


#ifdef LUA_REENTRANT

#define luaL_arg_check(L, cond,numarg,extramsg) if (!(cond)) \
                                               luaL_argerror(L, numarg,extramsg)
#define luaL_check_string(L,n)	(luaL_check_lstr(L, (n), NULL))
#define luaL_opt_string(L,n,d)	(luaL_opt_lstr(L, (n), (d), NULL))
#define luaL_check_int(L,n)	((int)luaL_check_number(L, n))
#define luaL_check_long(L,n)	((long)luaL_check_number(L, n))
#define luaL_opt_int(L,n,d)	((int)luaL_opt_number(L, n,d))
#define luaL_opt_long(L,n,d)	((long)luaL_opt_number(L, n,d))
#define luaL_openl(L,a)		luaL_openlib(L, a, (sizeof(a)/sizeof(a[0])))

#else

#define luaL_arg_check(cond,numarg,extramsg) if (!(cond)) \
                                               luaL_argerror(numarg,extramsg)
#define luaL_check_string(n)	(luaL_check_lstr((n), NULL))
#define luaL_opt_string(n, d)	(luaL_opt_lstr((n), (d), NULL))
#define luaL_check_int(n)	((int)luaL_check_number(n))
#define luaL_check_long(n)	((long)luaL_check_number(n))
#define luaL_opt_int(n,d)	((int)luaL_opt_number(n,d))
#define luaL_opt_long(n,d)	((long)luaL_opt_number(n,d))
#define luaL_openl(a)		luaL_openlib(a, (sizeof(a)/sizeof(a[0])))

#endif


void luaL_openlib (lua_State *L, const struct luaL_reg *l, int n);
void luaL_argerror (lua_State *L, int numarg, const char *extramsg);
const char *luaL_check_lstr (lua_State *L, int numArg, long *len);
const char *luaL_opt_lstr (lua_State *L, int numArg, const char *def, long *len);
double luaL_check_number (lua_State *L, int numArg);
double luaL_opt_number (lua_State *L, int numArg, double def);
lua_Object luaL_functionarg (lua_State *L, int arg);
lua_Object luaL_tablearg (lua_State *L, int arg);
lua_Object luaL_nonnullarg (lua_State *L, int numArg);
void luaL_verror (lua_State *L, const char *fmt, ...);
char *luaL_openspace (lua_State *L, int size);
void luaL_resetbuffer (lua_State *L);
void luaL_addchar (lua_State *L, int c);
int luaL_getsize (lua_State *L);
void luaL_addsize (lua_State *L, int n);
int luaL_newbuffer (lua_State *L, int size);
void luaL_oldbuffer (lua_State *L, int old);
char *luaL_buffer (lua_State *L);
int luaL_findstring (const char *name, const char *const list[]);
void luaL_chunkid (char *out, const char *source, int len);
void luaL_filesource (char *out, const char *filename, int len);


#ifndef LUA_REENTRANT

#define luaL_openlib(l,n)	(luaL_openlib)(lua_state,l,n)
#define luaL_argerror(numarg,extramsg)	\
				(luaL_argerror)(lua_state,numarg,extramsg)
#define luaL_check_lstr(numArg,len)	(luaL_check_lstr)(lua_state,numArg,len)
#define luaL_opt_lstr(numArg,def,len)	\
				(luaL_opt_lstr)(lua_state,numArg,def,len)
#define luaL_check_number(numArg)	(luaL_check_number)(lua_state,numArg)
#define luaL_opt_number(numArg,def)	(luaL_opt_number)(lua_state,numArg,def)
#define luaL_functionarg(arg)	(luaL_functionarg)(lua_state,arg)
#define luaL_tablearg(arg)	(luaL_tablearg)(lua_state,arg)
#define luaL_nonnullarg(numArg)	(luaL_nonnullarg)(lua_state,numArg)
#define luaL_openspace(size)	(luaL_openspace)(lua_state,size)
#define luaL_resetbuffer()	(luaL_resetbuffer)(lua_state)
#define luaL_addchar(c)		(luaL_addchar)(lua_state,c)
#define luaL_getsize()		(luaL_getsize)(lua_state)
#define luaL_addsize(n)		(luaL_addsize)(lua_state,n)
#define luaL_newbuffer(size)	(luaL_newbuffer)(lua_state,size)
#define luaL_oldbuffer(old)	(luaL_oldbuffer)(lua_state,old)
#define luaL_buffer()		(luaL_buffer)(lua_state)

#endif

#endif

