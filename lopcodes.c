/*
** $Id: lopcodes.c,v 1.2 2001/07/03 17:02:02 roberto Exp roberto $
** extracted automatically from lopcodes.h by mkprint.lua
** DO NOT EDIT
** See Copyright Notice in lua.h
*/


#define LUA_PRIVATE
#include "lua.h"

#include "lopcodes.h"


#ifdef LUA_OPNAMES

const l_char *const luaP_opnames[] = {
  l_s("MOVE"),
  l_s("LOADK"),
  l_s("LOADINT"),
  l_s("LOADNIL"),
  l_s("LOADUPVAL"),
  l_s("GETGLOBAL"),
  l_s("GETTABLE"),
  l_s("SETGLOBAL"),
  l_s("SETTABLE"),
  l_s("NEWTABLE"),
  l_s("SELF"),
  l_s("ADD"),
  l_s("SUB"),
  l_s("MUL"),
  l_s("DIV"),
  l_s("POW"),
  l_s("UNM"),
  l_s("NOT"),
  l_s("CONCAT"),
  l_s("JMP"),
  l_s("CJMP"),
  l_s("TESTEQ"),
  l_s("TESTNE"),
  l_s("TESTLT"),
  l_s("TESTLE"),
  l_s("TESTGT"),
  l_s("TESTGE"),
  l_s("TESTT"),
  l_s("TESTF"),
  l_s("NILJMP"),
  l_s("CALL"),
  l_s("RETURN"),
  l_s("FORPREP"),
  l_s("FORLOOP"),
  l_s("TFORPREP"),
  l_s("TFORLOOP"),
  l_s("SETLIST"),
  l_s("SETLISTO"),
  l_s("CLOSURE")
};

#endif

#define opmode(t,x,b,c,sa,k,m) (((t)<<OpModeT) | \
   ((b)<<OpModeBreg) | ((c)<<OpModeCreg) | \
   ((sa)<<OpModesetA) | ((k)<<OpModeK) | (m))

const lu_byte luaP_opmodes[NUM_OPCODES] = {
/*       T _ B C sA K mode		   opcode    */
  opmode(0,0,1,0, 1,0,iABC)		/* OP_MOVE */
 ,opmode(0,0,0,0, 1,1,iABc)		/* OP_LOADK */
 ,opmode(0,0,0,0, 1,0,iAsBc)		/* OP_LOADINT */
 ,opmode(0,0,1,0, 1,0,iABC)		/* OP_LOADNIL */
 ,opmode(0,0,0,0, 1,0,iABc)		/* OP_LOADUPVAL */
 ,opmode(0,0,0,0, 1,1,iABc)		/* OP_GETGLOBAL */
 ,opmode(0,0,1,1, 1,0,iABC)		/* OP_GETTABLE */
 ,opmode(0,0,0,0, 0,1,iABc)		/* OP_SETGLOBAL */
 ,opmode(0,0,1,1, 0,0,iABC)		/* OP_SETTABLE */
 ,opmode(0,0,0,0, 1,0,iABc)		/* OP_NEWTABLE */
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
 ,opmode(0,0,0,0, 0,0,iAsBc)		/* OP_CJMP */
 ,opmode(1,0,0,1, 0,0,iABC)		/* OP_TESTEQ */
 ,opmode(1,0,0,1, 0,0,iABC)		/* OP_TESTNE */
 ,opmode(1,0,0,1, 0,0,iABC)		/* OP_TESTLT */
 ,opmode(1,0,0,1, 0,0,iABC)		/* OP_TESTLE */
 ,opmode(1,0,0,1, 0,0,iABC)		/* OP_TESTGT */
 ,opmode(1,0,0,1, 0,0,iABC)		/* OP_TESTGE */
 ,opmode(1,0,1,0, 1,0,iABC)		/* OP_TESTT */
 ,opmode(1,0,1,0, 1,0,iABC)		/* OP_TESTF */
 ,opmode(0,0,0,0, 1,0,iABc)		/* OP_NILJMP */
 ,opmode(0,0,0,0, 0,0,iABC)		/* OP_CALL */
 ,opmode(0,0,0,0, 0,0,iABC)		/* OP_RETURN */
 ,opmode(0,0,0,0, 0,0,iAsBc)		/* OP_FORPREP */
 ,opmode(0,0,0,0, 0,0,iAsBc)		/* OP_FORLOOP */
 ,opmode(0,0,0,0, 0,0,iAsBc)		/* OP_TFORPREP */
 ,opmode(0,0,0,0, 0,0,iAsBc)		/* OP_TFORLOOP */
 ,opmode(0,0,0,0, 0,0,iABc)		/* OP_SETLIST */
 ,opmode(0,0,0,0, 0,0,iABc)		/* OP_SETLISTO */
 ,opmode(0,0,0,0, 1,0,iABc)		/* OP_CLOSURE */
};

