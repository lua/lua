/*
** $Id: lcode.h,v 1.12 2000/04/12 18:47:03 roberto Exp roberto $
** Code generator for Lua
** See Copyright Notice in lua.h
*/

#ifndef lcode_h
#define lcode_h

#include "llex.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lparser.h"


/*
** Marks the end of a patch list. It is an invalid value both as an absolute
** address, and as a list link (would link an element to itself).
*/
#define NO_JUMP (-1)


enum Mode {iO, iU, iS, iAB};  /* instruction format */

extern const struct OpProperties {
  char mode;
  signed char delta;
} luaK_opproperties[];


void luaK_error (LexState *ls, const char *msg);
int luaK_code0 (FuncState *fs, OpCode o);
int luaK_code1 (FuncState *fs, OpCode o, int arg1);
int luaK_code2 (FuncState *fs, OpCode o, int arg1, int arg2);
int luaK_jump (FuncState *fs);
void luaK_patchlist (FuncState *fs, int list, int target);
void luaK_concat (FuncState *fs, int *l1, int l2);
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
