/*
** $Id: lcode.h,v 1.1 2000/02/22 13:31:19 roberto Exp roberto $
** Code generator for Lua
** See Copyright Notice in lua.h
*/

#ifndef lcode_h
#define lcode_h

#include "llex.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lparser.h"


void luaK_error (LexState *ls, const char *msg);
int luaK_primitivecode (LexState *ls, Instruction i);
int luaK_code (LexState *ls, Instruction i);
void luaK_fixjump (LexState *ls, int pc, int dest);
void luaK_deltastack (LexState *ls, int delta);
int luaK_0 (LexState *ls, OpCode op, int delta);
int luaK_U (LexState *ls, OpCode op, int u, int delta);
int luaK_S (LexState *ls, OpCode op, int s, int delta);
int luaK_AB (LexState *ls, OpCode op, int a, int b, int delta);
int luaK_kstr (LexState *ls, int c);
int luaK_number (LexState *ls, real f);
int luaK_adjuststack (LexState *ls, int n);
int luaK_iscall (LexState *ls, int pc);
void luaK_setcallreturns (LexState *ls, int pc, int nresults);
void luaK_2stack (LexState *ls, expdesc *var);
void luaK_storevar (LexState *ls, const expdesc *var);
void luaK_prefix (LexState *ls, int op, expdesc *v);
void luaK_infix (LexState *ls, expdesc *v);
void luaK_posfix (LexState *ls, int op, expdesc *v1, expdesc *v2);


#endif
