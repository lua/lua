/*
** $Id: luadebug.h,v 1.25 2002/03/11 12:45:00 roberto Exp roberto $
** Debugging API
** See Copyright Notice in lua.h
*/


#ifndef luadebug_h
#define luadebug_h


#include "lua.h"

typedef struct lua_Debug lua_Debug;  /* activation record */

typedef void (*lua_Hook) (lua_State *L, lua_Debug *ar);


LUA_API int lua_getstack (lua_State *L, int level, lua_Debug *ar);
LUA_API int lua_getinfo (lua_State *L, const char *what, lua_Debug *ar);
LUA_API const char *lua_getlocal (lua_State *L, const lua_Debug *ar, int n);
LUA_API const char *lua_setlocal (lua_State *L, const lua_Debug *ar, int n);

LUA_API lua_Hook lua_setcallhook (lua_State *L, lua_Hook func);
LUA_API lua_Hook lua_setlinehook (lua_State *L, lua_Hook func);


#define LUA_IDSIZE	60

struct lua_Debug {
  const char *event;     /* `call', `return', `line' */
  const char *name;      /* (n) */
  const char *namewhat;  /* (n) `global', `tag method', `local', `field' */
  const char *what;      /* (S) `Lua' function, `C' function, Lua `main' */
  const char *source;    /* (S) */
  int currentline;       /* (l) */
  int nups;              /* (u) number of upvalues */
  int linedefined;       /* (S) */
  char short_src[LUA_IDSIZE]; /* (S) */
  /* private part */
  int i_ci;  /* active function */
};


#endif
