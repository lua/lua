/*
** $Id: lcode.h,v 1.3 2000/03/03 18:53:17 roberto Exp roberto $
** Code generator for Lua
** See Copyright Notice in lua.h
*/

#ifndef lcode_h
#define lcode_h

#include "llex.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lparser.h"


#define luaK_0(ls,o,d)		luaK_code(ls, CREATE_0(o), d)
#define luaK_U(ls,o,u,d)	luaK_code(ls, CREATE_U(o,u), d)
#define luaK_S(ls,o,s,d)	luaK_code(ls, CREATE_S(o,s), d)
#define luaK_AB(ls,o,a,b,d)	luaK_code(ls, CREATE_AB(o,a,b), d)


void luaK_error (LexState *ls, const char *msg);
int luaK_primitivecode (LexState *ls, Instruction i);
int luaK_code (LexState *ls, Instruction i, int delta);
void luaK_retcode (LexState *ls, int nlocals, int nexps);
void luaK_fixjump (LexState *ls, int pc, int dest);
int luaK_getlabel (LexState *ls);
void luaK_deltastack (LexState *ls, int delta);
void luaK_kstr (LexState *ls, int c);
void luaK_number (LexState *ls, real f);
void luaK_adjuststack (LexState *ls, int n);
int luaK_lastisopen (LexState *ls);
void luaK_setcallreturns (LexState *ls, int nresults);
void luaK_tostack (LexState *ls, expdesc *var);
void luaK_1tostack (LexState *ls, expdesc *var);
void luaK_storevar (LexState *ls, const expdesc *var);
void luaK_prefix (LexState *ls, int op, expdesc *v);
void luaK_infix (LexState *ls, int op, expdesc *v);
void luaK_posfix (LexState *ls, int op, expdesc *v1, expdesc *v2);


#endif
