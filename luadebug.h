/*
** LUA - Linguagem para Usuarios de Aplicacao
** Grupo de Tecnologia em Computacao Grafica
** TeCGraf - PUC-Rio
** $Id: luadebug.h,v 1.2 1995/10/26 14:21:56 roberto Exp $
*/


#ifndef luadebug_h
#define luadebug_h

#include "lua.h"

typedef void (*lua_LHFunction) (int line);
typedef void (*lua_CHFunction) (lua_Object func, char *file, int line);

lua_Object lua_stackedfunction(int level);
void lua_funcinfo (lua_Object func, char **filename, int *linedefined);
int lua_currentline (lua_Object func);
char *lua_getobjname (lua_Object o, char **name);
lua_LHFunction lua_setlinehook (lua_LHFunction hook);
lua_CHFunction lua_setcallhook (lua_CHFunction hook);

#endif
