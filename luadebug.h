/*
** $Id: luadebug.h,v 1.7 1999/08/16 20:52:00 roberto Exp roberto $
** Debugging API
** See Copyright Notice in lua.h
*/


#ifndef luadebug_h
#define luadebug_h


#include "lua.h"

typedef lua_Object lua_Function;

typedef void (*lua_LHFunction) (lua_State *L, int line);
typedef void (*lua_CHFunction) (lua_State *L, lua_Function func, const char *file, int line);

lua_Function lua_stackedfunction (lua_State *L, int level);
void lua_funcinfo (lua_State *L, lua_Object func, const char **source, int *linedefined);
int lua_currentline (lua_State *L, lua_Function func);
const char *lua_getobjname (lua_State *L, lua_Object o, const char **name);

lua_Object lua_getlocal (lua_State *L, lua_Function func, int local_number,
                         const char **name);
int lua_setlocal (lua_State *L, lua_Function func, int local_number);

int lua_nups (lua_State *L, lua_Function func);

lua_LHFunction lua_setlinehook (lua_State *L, lua_LHFunction func);
lua_CHFunction lua_setcallhook (lua_State *L, lua_CHFunction func);
int lua_setdebug (lua_State *L, int debug);


#endif
