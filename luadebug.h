/*
** LUA - Linguagem para Usuarios de Aplicacao
** Grupo de Tecnologia em Computacao Grafica
** TeCGraf - PUC-Rio
** $Id: luadebug.h,v 1.4 1996/02/07 18:10:27 roberto Exp roberto $
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
lua_LHFunction lua_setlinehook (lua_LHFunction hook);
lua_CHFunction lua_setcallhook (lua_CHFunction hook);

lua_Object lua_getlocal (lua_Function func, int local_number, char **name);
int lua_setlocal (lua_Function func, int local_number);

extern int lua_debug;

#endif
