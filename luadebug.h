/*
** $Id: luadebug.h,v 1.9 2000/01/19 12:00:45 roberto Exp roberto $
** Debugging API
** See Copyright Notice in lua.h
*/


#ifndef luadebug_h
#define luadebug_h


#include "lua.h"

typedef struct lua_Debug lua_Debug;  /* activation record */
typedef struct lua_Localvar lua_Localvar;

typedef void (*lua_Hook) (lua_State *L, lua_Debug *ar);


int lua_getstack (lua_State *L, int level, lua_Debug *ar);
int lua_getinfo (lua_State *L, const char *what, lua_Debug *ar);
int lua_getlocal (lua_State *L, const lua_Debug *ar, lua_Localvar *v);
int lua_setlocal (lua_State *L, const lua_Debug *ar, lua_Localvar *v);

int lua_setdebug (lua_State *L, int debug);

lua_Hook lua_setcallhook (lua_State *L, lua_Hook func);
lua_Hook lua_setlinehook (lua_State *L, lua_Hook func);



struct lua_Debug {
  const char *event;     /* `call', `return' */
  const char *source;    /* (S) */
  int linedefined;       /* (S) */
  const char *what;      /* (S) `Lua' function, `C' function, Lua `main' */
  int currentline;       /* (l) */
  const char *name;      /* (n) */
  const char *namewhat;  /* (n) global, tag method, local, field */
  int nups;              /* (u) number of upvalues */
  lua_Object func;       /* (f) function being executed */
  /* private part */
  lua_Object _func;  /* active function */
};


struct lua_Localvar {
  int index;
  const char *name;
  lua_Object value;
};


#endif
