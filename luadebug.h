/*
** LUA - Linguagem para Usuarios de Aplicacao
** Grupo de Tecnologia em Computacao Grafica
** TeCGraf - PUC-Rio
** $Id: $
*/


#ifndef luadebug_h
#define luadebug_h

#include "lua.h"

lua_Object lua_stackedfunction(int level);
void lua_funcinfo (lua_Object func, char **filename, char **funcname,
                    char **objname, int *linedefined);


#endif
