/*
** $Id: lcode.h,v 1.7 2000/03/13 20:37:16 roberto Exp roberto $
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
int luaK_code (FuncState *fs, Instruction i, int delta);
void luaK_retcode (FuncState *fs, int nlocals, int nexps);
void luaK_fixjump (FuncState *fs, int pc, int dest);
void luaK_patchlist (FuncState *fs, int list, int target);
void luaK_goiftrue (FuncState *fs, expdesc *v, int keepvalue);
void luaK_goiffalse (FuncState *fs, expdesc *v, int keepvalue);
int luaK_getlabel (FuncState *fs);
void luaK_deltastack (FuncState *fs, int delta);
void luaK_kstr (LexState *ls, int c);
void luaK_number (FuncState *fs, Number f);
void luaK_adjuststack (FuncState *fs, int n);
int luaK_lastisopen (FuncState *fs);
void luaK_setcallreturns (FuncState *fs, int nresults);
void luaK_tostack (LexState *ls, expdesc *v, int onlyone);
void luaK_storevar (LexState *ls, const expdesc *var);
void luaK_prefix (LexState *ls, int op, expdesc *v);
void luaK_infix (LexState *ls, int op, expdesc *v);
void luaK_posfix (LexState *ls, int op, expdesc *v1, expdesc *v2);


#endif
