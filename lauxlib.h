/*
** $Id: lauxlib.h,v 1.21 2000/08/29 20:43:28 roberto Exp roberto $
** Auxiliary functions for building Lua libraries
** See Copyright Notice in lua.h
*/


#ifndef lauxlib_h
#define lauxlib_h


#include <stddef.h>

#include "lua.h"


struct luaL_reg {
  const char *name;
  lua_CFunction func;
};


void luaL_openlib (lua_State *L, const struct luaL_reg *l, int n);
void luaL_argerror (lua_State *L, int numarg, const char *extramsg);
const char *luaL_check_lstr (lua_State *L, int numArg, size_t *len);
const char *luaL_opt_lstr (lua_State *L, int numArg, const char *def,
                           size_t *len);
double luaL_check_number (lua_State *L, int numArg);
double luaL_opt_number (lua_State *L, int numArg, double def);

void luaL_checkstack (lua_State *L, int space, const char *msg);
void luaL_checktype (lua_State *L, int narg, const char *tname);

void luaL_verror (lua_State *L, const char *fmt, ...);
int luaL_findstring (const char *name, const char *const list[]);
void luaL_chunkid (char *out, const char *source, int len);
void luaL_filesource (char *out, const char *filename, int len);


char *luaL_openspace (lua_State *L, size_t size);
void luaL_resetbuffer (lua_State *L);
void luaL_addchar (lua_State *L, int c);
size_t luaL_getsize (lua_State *L);
void luaL_addsize (lua_State *L, size_t n);
size_t luaL_newbuffer (lua_State *L, size_t size);
void luaL_oldbuffer (lua_State *L, size_t old);
char *luaL_buffer (lua_State *L);


/*
** ===============================================================
** some useful macros
** ===============================================================
*/

#define luaL_arg_check(L, cond,numarg,extramsg) if (!(cond)) \
                                               luaL_argerror(L, numarg,extramsg)
#define luaL_check_string(L,n)	(luaL_check_lstr(L, (n), NULL))
#define luaL_opt_string(L,n,d)	(luaL_opt_lstr(L, (n), (d), NULL))
#define luaL_check_int(L,n)	((int)luaL_check_number(L, n))
#define luaL_check_long(L,n)	((long)luaL_check_number(L, n))
#define luaL_opt_int(L,n,d)	((int)luaL_opt_number(L, n,d))
#define luaL_opt_long(L,n,d)	((long)luaL_opt_number(L, n,d))
#define luaL_openl(L,a)		luaL_openlib(L, a, (sizeof(a)/sizeof(a[0])))


#endif

