/*
** $Id: lparser.h,v 1.3 1999/02/25 19:13:56 roberto Exp roberto $
** LL(1) Parser and code generator for Lua
** See Copyright Notice in lua.h
*/

#ifndef lparser_h
#define lparser_h

#include "lobject.h"
#include "lzio.h"


void luaY_codedebugline (int line);
TProtoFunc *luaY_parser (ZIO *z);
void luaY_error (const char *s);
void luaY_syntaxerror (const char *s, const char *token);


#endif
