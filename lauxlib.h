/*
** $Id: lauxlib.h,v 1.26 2000/10/02 20:10:55 roberto Exp roberto $
** Auxiliary functions for building Lua libraries
** See Copyright Notice in lua.h
*/


#ifndef lauxlib_h
#define lauxlib_h


#include <stddef.h>
#include <stdio.h>

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
void luaL_checktype (lua_State *L, int narg, int t);
void luaL_checkany (lua_State *L, int narg);

void luaL_verror (lua_State *L, const char *fmt, ...);
int luaL_findstring (const char *name, const char *const list[]);



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


/*
** {======================================================
** Generic Buffer manipulation
** =======================================================
*/


#define LUAL_BUFFERSIZE	  BUFSIZ


typedef struct luaL_Buffer {
  char *p;			/* current position in buffer */
  int level;
  lua_State *L;
  char buffer[LUAL_BUFFERSIZE];
} luaL_Buffer;

#define luaL_putchar(B,c) \
  ((void)((B)->p < &(B)->buffer[LUAL_BUFFERSIZE] || luaL_prepbuffer(B)), \
   (*(B)->p++ = (char)(c)))

#define luaL_addsize(B,n)	((B)->p += (n))

void luaL_buffinit (lua_State *L, luaL_Buffer *B);
char *luaL_prepbuffer (luaL_Buffer *B);
void luaL_addlstring (luaL_Buffer *B, const char *s, size_t l);
void luaL_addstring (luaL_Buffer *B, const char *s);
void luaL_addvalue (luaL_Buffer *B);
void luaL_pushresult (luaL_Buffer *B);


/* }====================================================== */


#endif


