/*
** $Id: lopcodes.h,v 1.77 2001/07/03 17:01:34 roberto Exp roberto $
** Opcodes for Lua virtual machine
** See Copyright Notice in lua.h
*/

#ifndef lopcodes_h
#define lopcodes_h

#include "llimits.h"


/*===========================================================================
  We assume that instructions are unsigned numbers.
  All instructions have an opcode in the first 6 bits.
  Instructions can have the following fields:
	`A' : 8 bits (25-32)
	`B' : 8 bits (17-24)
	`C' : 10 bits (7-16)
	`Bc' : 18 bits (`B' and `C' together)
	`sBc' : signed Bc

  A signed argument is represented in excess K; that is, the number
  value is the unsigned value minus K. K is exactly the maximum value
  for that argument (so that -max is represented by 0, and +max is
  represented by 2*max), which is half the maximum for the corresponding
  unsigned argument.
===========================================================================*/


enum OpMode {iABC, iABc, iAsBc};  /* basic instruction format */


/*
** size and position of opcode arguments.
*/
#define SIZE_C		10
#define SIZE_B		8
#define SIZE_Bc		(SIZE_C + SIZE_B)
#define SIZE_A		8

#define SIZE_OP		6

#define POS_C		SIZE_OP
#define POS_B		(POS_C + SIZE_C)
#define POS_Bc		POS_C
#define POS_A		(POS_B + SIZE_B)


/*
** limits for opcode arguments.
** we use (signed) int to manipulate most arguments,
** so they must fit in BITS_INT-1 bits (-1 for sign)
*/
#if SIZE_Bc < BITS_INT-1
#define MAXARG_Bc        ((1<<SIZE_Bc)-1)
#define MAXARG_sBc        (MAXARG_Bc>>1)         /* `sBc' is signed */
#else
#define MAXARG_Bc        MAX_INT
#define MAXARG_sBc        MAX_INT
#endif


#define MAXARG_A        ((1<<SIZE_A)-1)
#define MAXARG_B        ((1<<SIZE_B)-1)
#define MAXARG_C        ((1<<SIZE_C)-1)


/* creates a mask with `n' 1 bits at position `p' */
#define MASK1(n,p)	((~((~(Instruction)0)<<n))<<p)

/* creates a mask with `n' 0 bits at position `p' */
#define MASK0(n,p)	(~MASK1(n,p))

/*
** the following macros help to manipulate instructions
*/

#define GET_OPCODE(i)	((OpCode)((i)&MASK1(SIZE_OP,0)))
#define SET_OPCODE(i,o)	(((i)&MASK0(SIZE_OP,0)) | (Instruction)(o))

#define GETARG_A(i)	((int)((i)>>POS_A))
#define SETARG_A(i,u)	((i) = (((i)&MASK0(SIZE_A,POS_A)) | \
                               ((Instruction)(u)<<POS_A)))

#define GETARG_B(i)	((int)(((i)>>POS_B) & MASK1(SIZE_B,0)))
#define SETARG_B(i,b)	((i) = (((i)&MASK0(SIZE_B,POS_B)) | \
                               ((Instruction)(b)<<POS_B)))

#define GETARG_C(i)	((int)(((i)>>POS_C) & MASK1(SIZE_C,0)))
#define SETARG_C(i,b)	((i) = (((i)&MASK0(SIZE_C,POS_C)) | \
                               ((Instruction)(b)<<POS_C)))

#define GETARG_Bc(i)	((int)(((i)>>POS_Bc) & MASK1(SIZE_Bc,0)))
#define SETARG_Bc(i,b)	((i) = (((i)&MASK0(SIZE_Bc,POS_Bc)) | \
                               ((Instruction)(b)<<POS_Bc)))

#define GETARG_sBc(i)	(GETARG_Bc(i)-MAXARG_sBc)
#define SETARG_sBc(i,b)	SETARG_Bc((i),(unsigned int)((b)+MAXARG_sBc))


#define CREATE_ABC(o,a,b,c)	((Instruction)(o) \
			| ((Instruction)(a)<<POS_A) \
			| ((Instruction)(b)<<POS_B) \
			| ((Instruction)(c)<<POS_C))

#define CREATE_ABc(o,a,bc)	((Instruction)(o) \
			| ((Instruction)(a)<<POS_A) \
			| ((Instruction)(bc)<<POS_Bc))




/*
** an invalid register that fits in 8 bits
*/
#define NO_REG		MAXARG_A


/*
** R(x) - register
** Kst(x) - constant (in constant table)
** R/K(x) == if x < MAXSTACK then R(x) else Kst(x-MAXSTACK)
*/

typedef enum {
/*----------------------------------------------------------------------
name		args	description
------------------------------------------------------------------------*/
OP_MOVE,/*	A B	R(A) := R(B)					*/
OP_LOADK,/*	A Bc	R(A) := Kst(Bc)					*/
OP_LOADINT,/*	A sBc	R(A) := (Number)sBc				*/
OP_LOADNIL,/*	A B	R(A) := ... := R(B) := nil			*/
OP_LOADUPVAL,/*	A Bc	R(A) := UpValue[Bc]				*/

OP_GETGLOBAL,/*	A Bc	R(A) := Gbl[Kst(Bc)]				*/
OP_GETTABLE,/*	A B C	R(A) := R(B)[R/K(C)]				*/

OP_SETGLOBAL,/*	A Bc	Gbl[Kst(Bc)] := R(A)				*/
OP_SETTABLE,/*	A B C	R(B)[R/K(C)] := R(A)				*/

OP_NEWTABLE,/*	A Bc	R(A) := {} (size = Bc)				*/

OP_SELF,/*	A B C	R(A+1) := R(B); R(A) := R(B)[R/K(C)]		*/

OP_ADD,/*	A B C	R(A) := R(B) + R/K(C)				*/
OP_SUB,/*	A B C	R(A) := R(B) - R/K(C)				*/
OP_MUL,/*	A B C	R(A) := R(B) * R/K(C)				*/
OP_DIV,/*	A B C	R(A) := R(B) / R/K(C)				*/
OP_POW,/*	A B C	R(A) := R(B) ^ R/K(C)				*/
OP_UNM,/*	A B	R(A) := -R(B)					*/
OP_NOT,/*	A B	R(A) := not R(B)				*/

OP_CONCAT,/*	A B C	R(A) := R(B).. ... ..R(C)			*/

OP_JMP,/*	sBc	PC += sBc					*/
OP_CJMP,/*	sBc	if test then PC += sBc		(see (1))	*/

OP_TESTEQ,/*	A C	test := (R(A) == R/K(C))			*/
OP_TESTNE,/*	A C	test := (R(A) ~= R/K(C))			*/
OP_TESTLT,/*	A C	test := (R(A) < R/K(C))				*/
OP_TESTLE,/*	A C	test := (R(A) <= R/K(C))			*/
OP_TESTGT,/*	A C	test := (R(A) > R/K(C))				*/
OP_TESTGE,/*	A C	test := (R(A) >= R/K(C))			*/

OP_TESTT,/*	A B	test := R(B); if (test) R(A) := R(B)		*/
OP_TESTF,/*	A B	test := not R(B); if (test) R(A) := nil		*/

OP_NILJMP,/*	A Bc	R(A) := nil; PC++;				*/

OP_CALL,/*	A B C	R(A), ... ,R(A+C-1) := R(A)(R(A+1), ... ,R(A+B))*/
OP_RETURN,/*	A B	return R(A), ... ,R(A+B-1)	(see (3))	*/

OP_FORPREP,/*	A sBc							*/
OP_FORLOOP,/*	A sBc							*/

OP_TFORPREP,/*	A sBc							*/
OP_TFORLOOP,/*	A sBc							*/

OP_SETLIST,/*	A Bc	R(A)[Bc-Bc%FPF+i] := R(A+i), 1 <= i <= Bc%FPF+1	*/
OP_SETLISTO,/*	A Bc							*/

OP_CLOSURE /*	A Bc	R(A) := closure(KPROTO[Bc], R(A), ... ,R(A+n))	*/
} OpCode;


#define NUM_OPCODES	((int)OP_CLOSURE+1)



/*===========================================================================
  Notes:
  (1) In the current implementation there is no `test' variable;
      instructions OP_TEST* and OP_CJMP must always occur together.

  (2) In OP_CALL, if (B == NO_REG) then B = top. C is the number of returns,
      and can be NO_REG. OP_CALL can set `top' to last_result+1, so
      next open instruction (OP_CALL, OP_RETURN, OP_SETLIST) may use `top'.

  (3) In OP_RETURN, if (B == NO_REG) then return up to `top'
===========================================================================*/



/*
** masks for instruction properties
*/  
enum OpModeMask {
  OpModeBreg = 2,       /* B is a register */
  OpModeCreg,           /* C is a register/constant */
  OpModesetA,           /* instruction set register A */
  OpModeK,              /* Bc is a constant */
  OpModeT               /* operator is a test */
};

extern const lu_byte luaP_opmodes[];

#define getOpMode(m)            ((enum OpMode)(luaP_opmodes[m] & 3))
#define testOpMode(m, b)        (luaP_opmodes[m] & (1 << (b)))


/*
** opcode names (only included when compiled with LUA_OPNAMES)
*/
extern const l_char *const luaP_opnames[];


#endif
