/*
** $Id: lauxlib.h,v 1.33 2001/02/02 19:02:40 roberto Exp roberto $
** Auxiliary functions for building Lua libraries
** See Copyright Notice in lua.h
*/


#ifndef lauxlib_h
#define lauxlib_h


#include <stddef.h>
#include <stdio.h>

#include "lua.h"


#ifndef LUALIB_API
#define LUALIB_API	extern
#endif


typedef struct luaL_reg {
  const l_char *name;
  lua_CFunction func;
} luaL_reg;


LUALIB_API void luaL_openlib (lua_State *L, const luaL_reg *l, int n);
LUALIB_API void luaL_argerror (lua_State *L, int numarg, const l_char *extramsg);
LUALIB_API const l_char *luaL_check_lstr (lua_State *L, int numArg, size_t *len);
LUALIB_API const l_char *luaL_opt_lstr (lua_State *L, int numArg, const l_char *def, size_t *len);
LUALIB_API lua_Number luaL_check_number (lua_State *L, int numArg);
LUALIB_API lua_Number luaL_opt_number (lua_State *L, int nArg, lua_Number def);

LUALIB_API void luaL_checkstack (lua_State *L, int space, const l_char *msg);
LUALIB_API void luaL_checktype (lua_State *L, int narg, int t);
LUALIB_API void luaL_checkany (lua_State *L, int narg);
LUALIB_API void *luaL_check_userdata (lua_State *L, int narg, const l_char *name);

LUALIB_API void luaL_verror (lua_State *L, const l_char *fmt, ...);
LUALIB_API int luaL_findstring (const l_char *name, const l_char *const list[]);



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


#ifndef LUAL_BUFFERSIZE
#define LUAL_BUFFERSIZE	  BUFSIZ
#endif


typedef struct luaL_Buffer {
  l_char *p;			/* current position in buffer */
  int level;
  lua_State *L;
  l_char buffer[LUAL_BUFFERSIZE];
} luaL_Buffer;

#define luaL_putchar(B,c) \
  ((void)((B)->p < &(B)->buffer[LUAL_BUFFERSIZE] || luaL_prepbuffer(B)), \
   (*(B)->p++ = (l_char)(c)))

#define luaL_addsize(B,n)	((B)->p += (n))

LUALIB_API void luaL_buffinit (lua_State *L, luaL_Buffer *B);
LUALIB_API l_char *luaL_prepbuffer (luaL_Buffer *B);
LUALIB_API void luaL_addlstring (luaL_Buffer *B, const l_char *s, size_t l);
LUALIB_API void luaL_addstring (luaL_Buffer *B, const l_char *s);
LUALIB_API void luaL_addvalue (luaL_Buffer *B);
LUALIB_API void luaL_pushresult (luaL_Buffer *B);


/* }====================================================== */


#endif


