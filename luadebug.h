/*
** $Id: luadebug.h,v 1.19 2001/03/07 18:09:25 roberto Exp roberto $
** Debugging API
** See Copyright Notice in lua.h
*/


#ifndef luadebug_h
#define luadebug_h


#include "lua.h"

typedef struct lua_Debug lua_Debug;  /* activation record */
typedef struct lua_Localvar lua_Localvar;

typedef void (*lua_Hook) (lua_State *L, lua_Debug *ar);


LUA_API int lua_getstack (lua_State *L, int level, lua_Debug *ar);
LUA_API int lua_getinfo (lua_State *L, const lua_char *what, lua_Debug *ar);
LUA_API const lua_char *lua_getlocal (lua_State *L, const lua_Debug *ar, int n);
LUA_API const lua_char *lua_setlocal (lua_State *L, const lua_Debug *ar, int n);

LUA_API lua_Hook lua_setcallhook (lua_State *L, lua_Hook func);
LUA_API lua_Hook lua_setlinehook (lua_State *L, lua_Hook func);


#define LUA_IDSIZE	60

struct lua_Debug {
  const lua_char *event;     /* `call', `return' */
  int currentline;       /* (l) */
  const lua_char *name;      /* (n) */
  const lua_char *namewhat;  /* (n) `global', `tag method', `local', `field' */
  int nups;              /* (u) number of upvalues */
  int linedefined;       /* (S) */
  const lua_char *what;      /* (S) `Lua' function, `C' function, Lua `main' */
  const lua_char *source;    /* (S) */
  lua_char short_src[LUA_IDSIZE]; /* (S) */
  /* private part */
  struct CallInfo *_ci;  /* active function */
};


#endif
