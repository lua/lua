/*
** $Id: luadebug.h,v 1.8 1999/11/22 13:12:07 roberto Exp roberto $
** Debugging API
** See Copyright Notice in lua.h
*/


#ifndef luadebug_h
#define luadebug_h


#include "lua.h"

typedef struct lua_Dbgactreg lua_Dbgactreg;  /* activation record */
typedef struct lua_Dbglocvar lua_Dbglocvar;  /* local variable */

typedef void (*lua_Dbghook) (lua_State *L, lua_Dbgactreg *ar);


int lua_getstack (lua_State *L, int level, lua_Dbgactreg *ar);
int lua_getinfo (lua_State *L, const char *what, lua_Dbgactreg *ar);
int lua_getlocal (lua_State *L, const lua_Dbgactreg *ar, lua_Dbglocvar *v);
int lua_setlocal (lua_State *L, const lua_Dbgactreg *ar, lua_Dbglocvar *v);

int lua_setdebug (lua_State *L, int debug);

lua_Dbghook lua_setcallhook (lua_State *L, lua_Dbghook func);
lua_Dbghook lua_setlinehook (lua_State *L, lua_Dbghook func);



struct lua_Dbgactreg {
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


struct lua_Dbglocvar {
  int index;
  const char *name;
  lua_Object value;
};


#endif
