/*
** $Id: lopcodes.c,v 1.68 2017/11/16 12:59:14 roberto Exp roberto $
** Opcodes for Lua virtual machine
** See Copyright Notice in lua.h
*/

#define lopcodes_c
#define LUA_CORE

#include "lprefix.h"


#include <stddef.h>

#include "lopcodes.h"


/* ORDER OP */

LUAI_DDEF const char *const luaP_opnames[NUM_OPCODES+1] = {
  "MOVE",
  "LOADI",
  "LOADF",
  "LOADK",
  "LOADKX",
  "LOADBOOL",
  "LOADNIL",
  "GETUPVAL",
  "SETUPVAL",
  "GETTABUP",
  "GETTABLE",
  "GETI",
  "GETFIELD",
  "SETTABUP",
  "SETTABLE",
  "SETI",
  "SETFIELD",
  "NEWTABLE",
  "SELF",
  "ADDI",
  "SUBI",
  "MULI",
  "MODI",
  "POWI",
  "DIVI",
  "IDIVI",
  "ADD",
  "SUB",
  "MUL",
  "MOD",
  "POW",
  "DIV",
  "IDIV",
  "BAND",
  "BOR",
  "BXOR",
  "SHL",
  "SHR",
  "UNM",
  "BNOT",
  "NOT",
  "LEN",
  "CONCAT",
  "CLOSE",
  "JMP",
  "EQ",
  "LT",
  "LE",
  "EQK",
  "EQI",
  "TEST",
  "TESTSET",
  "CALL",
  "TAILCALL",
  "RETURN",
  "FORLOOP",
  "FORPREP",
  "TFORCALL",
  "TFORLOOP",
  "SETLIST",
  "CLOSURE",
  "VARARG",
  "EXTRAARG",
  NULL
};


LUAI_DDEF const lu_byte luaP_opmodes[NUM_OPCODES] = {
/*       T  A    mode		   opcode	*/
  opmode(0, 1, iABC)		/* OP_MOVE */
 ,opmode(0, 1, iAsBx)		/* OP_LOADI */
 ,opmode(0, 1, iAsBx)		/* OP_LOADF */
 ,opmode(0, 1, iABx)		/* OP_LOADK */
 ,opmode(0, 1, iABx)		/* OP_LOADKX */
 ,opmode(0, 1, iABC)		/* OP_LOADBOOL */
 ,opmode(0, 1, iABC)		/* OP_LOADNIL */
 ,opmode(0, 1, iABC)		/* OP_GETUPVAL */
 ,opmode(0, 0, iABC)		/* OP_SETUPVAL */
 ,opmode(0, 1, iABC)		/* OP_GETTABUP */
 ,opmode(0, 1, iABC)		/* OP_GETTABLE */
 ,opmode(0, 1, iABC)		/* OP_GETI */
 ,opmode(0, 1, iABC)		/* OP_GETFIELD */
 ,opmode(0, 0, iABC)		/* OP_SETTABUP */
 ,opmode(0, 0, iABC)		/* OP_SETTABLE */
 ,opmode(0, 0, iABC)		/* OP_SETI */
 ,opmode(0, 0, iABC)		/* OP_SETFIELD */
 ,opmode(0, 1, iABC)		/* OP_NEWTABLE */
 ,opmode(0, 1, iABC)		/* OP_SELF */
 ,opmode(0, 1, iABC)		/* OP_ADDI */
 ,opmode(0, 1, iABC)		/* OP_SUBI */
 ,opmode(0, 1, iABC)		/* OP_MULI */
 ,opmode(0, 1, iABC)		/* OP_MODI */
 ,opmode(0, 1, iABC)		/* OP_POWI */
 ,opmode(0, 1, iABC)		/* OP_DIVI */
 ,opmode(0, 1, iABC)		/* OP_IDIVI */
 ,opmode(0, 1, iABC)		/* OP_ADD */
 ,opmode(0, 1, iABC)		/* OP_SUB */
 ,opmode(0, 1, iABC)		/* OP_MUL */
 ,opmode(0, 1, iABC)		/* OP_MOD */
 ,opmode(0, 1, iABC)		/* OP_POW */
 ,opmode(0, 1, iABC)		/* OP_DIV */
 ,opmode(0, 1, iABC)		/* OP_IDIV */
 ,opmode(0, 1, iABC)		/* OP_BAND */
 ,opmode(0, 1, iABC)		/* OP_BOR */
 ,opmode(0, 1, iABC)		/* OP_BXOR */
 ,opmode(0, 1, iABC)		/* OP_SHL */
 ,opmode(0, 1, iABC)		/* OP_SHR */
 ,opmode(0, 1, iABC)		/* OP_UNM */
 ,opmode(0, 1, iABC)		/* OP_BNOT */
 ,opmode(0, 1, iABC)		/* OP_NOT */
 ,opmode(0, 1, iABC)		/* OP_LEN */
 ,opmode(0, 1, iABC)		/* OP_CONCAT */
 ,opmode(0, 0, iABC)		/* OP_CLOSE */
 ,opmode(0, 0, isJ)		/* OP_JMP */
 ,opmode(1, 0, iABC)		/* OP_EQ */
 ,opmode(1, 0, iABC)		/* OP_LT */
 ,opmode(1, 0, iABC)		/* OP_LE */
 ,opmode(1, 0, iABC)		/* OP_EQK */
 ,opmode(1, 0, iABC)		/* OP_EQI */
 ,opmode(1, 0, iABC)		/* OP_TEST */
 ,opmode(1, 1, iABC)		/* OP_TESTSET */
 ,opmode(0, 1, iABC)		/* OP_CALL */
 ,opmode(0, 1, iABC)		/* OP_TAILCALL */
 ,opmode(0, 0, iABC)		/* OP_RETURN */
 ,opmode(0, 1, iABx)		/* OP_FORLOOP */
 ,opmode(0, 1, iABx)		/* OP_FORPREP */
 ,opmode(0, 0, iABC)		/* OP_TFORCALL */
 ,opmode(0, 1, iABx)		/* OP_TFORLOOP */
 ,opmode(0, 0, iABC)		/* OP_SETLIST */
 ,opmode(0, 1, iABx)		/* OP_CLOSURE */
 ,opmode(0, 1, iABC)		/* OP_VARARG */
 ,opmode(0, 0, iAx)		/* OP_EXTRAARG */
};

