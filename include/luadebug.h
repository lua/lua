/*
** LUA - Linguagem para Usuarios de Aplicacao
** Grupo de Tecnologia em Computacao Grafica
** TeCGraf - PUC-Rio
** $Id: luadebug.h,v 1.6 1996/03/20 17:05:44 roberto Exp $
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

extern lua_LHFunction lua_linehook;
extern lua_CHFunction lua_callhook;
extern int lua_debug;

#endif
