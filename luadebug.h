/*
** $Id: luadebug.h,v 1.29 2002/07/08 18:21:33 roberto Exp roberto $
** Debugging API
** See Copyright Notice in lua.h
*/


#ifndef luadebug_h
#define luadebug_h


#include "lua.h"

typedef enum lua_Hookevent {
  LUA_HOOKCALL, LUA_HOOKRET, LUA_HOOKLINE, LUA_HOOKCOUNT
} lua_Hookevent;


#define LUA_MASKCALL	(2 << LUA_HOOKCALL)
#define LUA_MASKRET	(2 << LUA_HOOKRET)
#define LUA_MASKLINE	(2 << LUA_HOOKLINE)
#define LUA_MASKCOUNT(count)	((count) << (LUA_HOOKCOUNT+1))
#define lua_getmaskcount(mask)	((mask) >> (LUA_HOOKCOUNT+1))

typedef struct lua_Debug lua_Debug;  /* activation record */

typedef void (*lua_Hook) (lua_State *L, lua_Debug *ar);


LUA_API int lua_getstack (lua_State *L, int level, lua_Debug *ar);
LUA_API int lua_getinfo (lua_State *L, const char *what, lua_Debug *ar);
LUA_API const char *lua_getlocal (lua_State *L, const lua_Debug *ar, int n);
LUA_API const char *lua_setlocal (lua_State *L, const lua_Debug *ar, int n);

LUA_API int lua_sethook (lua_State *L, lua_Hook func, int mask);
LUA_API lua_Hook lua_gethook (lua_State *L);
LUA_API int lua_gethookmask (lua_State *L);


#define LUA_IDSIZE	60

struct lua_Debug {
  lua_Hookevent event;
  const char *name;	/* (n) */
  const char *namewhat;	/* (n) `global', `local', `field', `method' */
  const char *what;	/* (S) `Lua' function, `C' function, Lua `main' */
  const char *source;	/* (S) */
  int currentline;	/* (l) */
  int isprotected;	/* (c) function was called in protected mode */
  int nups;		/* (u) number of upvalues */
  int linedefined;	/* (S) */
  char short_src[LUA_IDSIZE]; /* (S) */
  /* private part */
  int i_ci;  /* active function */
};


#endif
