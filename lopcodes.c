/*
** $Id: lopcodes.c,v 1.15 2002/04/09 19:47:44 roberto Exp roberto $
** extracted automatically from lopcodes.h by mkprint.lua
** DO NOT EDIT
** See Copyright Notice in lua.h
*/


#include "lua.h"

#include "lobject.h"
#include "lopcodes.h"


#ifdef LUA_OPNAMES

const char *const luaP_opnames[] = {
  "MOVE",
  "LOADK",
  "LOADBOOL",
  "LOADNIL",
  "GETUPVAL",
  "GETGLOBAL",
  "GETTABLE",
  "SETGLOBAL",
  "SETUPVAL",
  "SETTABLE",
  "NEWTABLE",
  "SELF",
  "ADD",
  "SUB",
  "MUL",
  "DIV",
  "POW",
  "UNM",
  "NOT",
  "CONCAT",
  "JMP",
  "TESTEQ",
  "TESTNE",
  "TESTLT",
  "TESTLE",
  "TESTGT",
  "TESTGE",
  "TESTT",
  "TESTF",
  "CALL",
  "TAILCALL",
  "RETURN",
  "FORLOOP",
  "TFORLOOP",
  "TFORPREP",
  "SETLIST",
  "SETLISTO",
  "CLOSE",
  "CLOSURE"
};

#endif

#define opmode(t,x,b,c,sa,k,m) (((t)<<OpModeT) | \
   ((b)<<OpModeBreg) | ((c)<<OpModeCreg) | \
   ((sa)<<OpModesetA) | ((k)<<OpModeK) | (m))


const lu_byte luaP_opmodes[NUM_OPCODES] = {
/*       T _ B C sA K mode		   opcode    */
  opmode(0,0,1,0, 1,0,iABC)		/* OP_MOVE */
 ,opmode(0,0,0,0, 1,1,iABc)		/* OP_LOADK */
 ,opmode(0,0,0,0, 1,0,iABC)		/* OP_LOADBOOL */
 ,opmode(0,0,1,0, 1,0,iABC)		/* OP_LOADNIL */
 ,opmode(0,0,0,0, 1,0,iABC)		/* OP_GETUPVAL */
 ,opmode(0,0,0,0, 1,1,iABc)		/* OP_GETGLOBAL */
 ,opmode(0,0,1,1, 1,0,iABC)		/* OP_GETTABLE */
 ,opmode(0,0,0,0, 0,1,iABc)		/* OP_SETGLOBAL */
 ,opmode(0,0,0,0, 0,0,iABC)		/* OP_SETUPVAL */
 ,opmode(0,0,1,1, 0,0,iABC)		/* OP_SETTABLE */
 ,opmode(0,0,0,0, 1,0,iABC)		/* OP_NEWTABLE */
 ,opmode(0,0,1,1, 1,0,iABC)		/* OP_SELF */
 ,opmode(0,0,1,1, 1,0,iABC)		/* OP_ADD */
 ,opmode(0,0,1,1, 1,0,iABC)		/* OP_SUB */
 ,opmode(0,0,1,1, 1,0,iABC)		/* OP_MUL */
 ,opmode(0,0,1,1, 1,0,iABC)		/* OP_DIV */
 ,opmode(0,0,1,1, 1,0,iABC)		/* OP_POW */
 ,opmode(0,0,1,0, 1,0,iABC)		/* OP_UNM */
 ,opmode(0,0,1,0, 1,0,iABC)		/* OP_NOT */
 ,opmode(0,0,1,1, 1,0,iABC)		/* OP_CONCAT */
 ,opmode(0,0,0,0, 0,0,iAsBc)		/* OP_JMP */
 ,opmode(1,0,0,1, 0,0,iABC)		/* OP_TESTEQ */
 ,opmode(1,0,0,1, 0,0,iABC)		/* OP_TESTNE */
 ,opmode(1,0,0,1, 0,0,iABC)		/* OP_TESTLT */
 ,opmode(1,0,0,1, 0,0,iABC)		/* OP_TESTLE */
 ,opmode(1,0,0,1, 0,0,iABC)		/* OP_TESTGT */
 ,opmode(1,0,0,1, 0,0,iABC)		/* OP_TESTGE */
 ,opmode(1,0,1,0, 1,0,iABC)		/* OP_TESTT */
 ,opmode(1,0,1,0, 1,0,iABC)		/* OP_TESTF */
 ,opmode(0,0,0,0, 0,0,iABC)		/* OP_CALL */
 ,opmode(0,0,0,0, 0,0,iABC)		/* OP_TAILCALL */
 ,opmode(0,0,0,0, 0,0,iABC)		/* OP_RETURN */
 ,opmode(0,0,0,0, 0,0,iAsBc)		/* OP_FORLOOP */
 ,opmode(0,0,0,0, 0,0,iABC)		/* OP_TFORLOOP */
 ,opmode(0,0,0,0, 0,0,iABC)		/* OP_TFORPREP */
 ,opmode(0,0,0,0, 0,0,iABc)		/* OP_SETLIST */
 ,opmode(0,0,0,0, 0,0,iABc)		/* OP_SETLISTO */
 ,opmode(0,0,0,0, 0,0,iABC)		/* OP_CLOSE */
 ,opmode(0,0,0,0, 1,0,iABc)		/* OP_CLOSURE */
};

