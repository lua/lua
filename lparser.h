/*
** $Id: lparser.h,v 1.4 1999/08/16 20:52:00 roberto Exp roberto $
** LL(1) Parser and code generator for Lua
** See Copyright Notice in lua.h
*/

#ifndef lparser_h
#define lparser_h

#include "lobject.h"
#include "lzio.h"


TProtoFunc *luaY_parser (lua_State *L, ZIO *z);


#endif
