/*
** $Id: luadebug.h,v 1.6 1999/03/04 21:17:26 roberto Exp roberto $
** Debugging API
** See Copyright Notice in lua.h
*/


#ifndef luadebug_h
#define luadebug_h


#include "lua.h"

typedef lua_Object lua_Function;

typedef void (*lua_LHFunction) (int line);
typedef void (*lua_CHFunction) (lua_Function func, const char *file, int line);

lua_Function lua_stackedfunction (int level);
void lua_funcinfo (lua_Object func, const char **source, int *linedefined);
int lua_currentline (lua_Function func);
const char *lua_getobjname (lua_Object o, const char **name);

lua_Object lua_getlocal (lua_Function func, int local_number,
                         const char **name);
int lua_setlocal (lua_Function func, int local_number);

int lua_nups (lua_Function func);

lua_LHFunction lua_setlinehook (lua_LHFunction func);
lua_CHFunction lua_setcallhook (lua_CHFunction func);
int lua_setdebug (int debug);


#endif
