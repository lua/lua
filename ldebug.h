/*
** $Id: ldebug.h,v 1.12 2001/06/05 18:17:01 roberto Exp roberto $
** Auxiliary functions from Debug Interface module
** See Copyright Notice in lua.h
*/

#ifndef ldebug_h
#define ldebug_h


#include "lstate.h"
#include "luadebug.h"


enum OpMode {iABC, iABc, iAsBc};  /* basic instruction format */

/*
** masks for instruction properties
*/
enum OpModeMask {
  OpModeAreg = 2,	/* A is a register */
  OpModeBreg,		/* B is a register */
  OpModeCreg,		/* C is a register/constant */
  OpModesetA,		/* instruction set register A */
  OpModeK,		/* Bc is a constant */
  OpModeT		/* operator is a test */
};

extern const lu_byte luaG_opmodes[];

#define getOpMode(m)		((enum OpMode)(luaG_opmodes[m] & 3))
#define testOpMode(m, b)	(luaG_opmodes[m] & (1 << (b)))


void luaG_typeerror (lua_State *L, StkId o, const l_char *op);
void luaG_concaterror (lua_State *L, StkId p1, StkId p2);
void luaG_aritherror (lua_State *L, StkId p1, TObject *p2);
int luaG_getline (int *lineinfo, int pc, int refline, int *refi);
void luaG_ordererror (lua_State *L, const TObject *p1, const TObject *p2);
int luaG_checkcode (const Proto *pt);


#endif
