/*
** $Id: luadebug.h,v 1.3 1998/09/07 18:59:59 roberto Exp roberto $
** Debugging API
** See Copyright Notice in lua.h
*/


#ifndef luadebug_h
#define luadebug_h


#include "lua.h"

typedef lua_Object lua_Function;

typedef void (*lua_LHFunction) (int line);
typedef void (*lua_CHFunction) (lua_Function func, char *file, int line);

lua_Function lua_stackedfunction (int level);
void lua_funcinfo (lua_Object func, char **filename, int *linedefined);
int lua_currentline (lua_Function func);
char *lua_getobjname (lua_Object o, char **name);

lua_Object lua_getlocal (lua_Function func, int local_number, char **name);
int lua_setlocal (lua_Function func, int local_number);

int lua_nups (lua_Function func);

extern lua_LHFunction lua_linehook;
extern lua_CHFunction lua_callhook;
extern int lua_debug;


extern lua_LHFunction lua_setlinehook (lua_LHFunction func);
extern lua_CHFunction lua_setcallhook (lua_CHFunction func);
extern int lua_setdebug (int debug);


#endif
