/*
** $Id: $
** Code generator for Lua
** See Copyright Notice in lua.h
*/

#ifndef lcode_h
#define lcode_h

#include "llex.h"
#include "lobject.h"


int luaK_primitivecode (LexState *ls, Instruction i);
int luaK_code (LexState *ls, Instruction i);
void luaK_fixjump (LexState *ls, int pc, int dest);


#endif
