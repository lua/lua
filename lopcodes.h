/*
** $Id: lopcodes.h,v 1.180 2017/12/18 17:49:31 roberto Exp roberto $
** Opcodes for Lua virtual machine
** See Copyright Notice in lua.h
*/

#ifndef lopcodes_h
#define lopcodes_h

#include "llimits.h"


/*===========================================================================
  We assume that instructions are unsigned 32-bit integers.
  All instructions have an opcode in the first 7 bits.
  Instructions can have the following formats:

        3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0
        1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
iABC    |k|     C(8)    | |     B(8)    | |     A(8)    | |   Op(7)   |
iABx    |            Bx(17)             | |     A(8)    | |   Op(7)   |
iAsBx   |           sBx (signed)(17)    | |     A(8)    | |   Op(7)   |
iAx     |                       Ax(25)                  | |   Op(7)   |
iksJ    |k|                     sJ(24)                  | |   Op(7)   |

  A signed argument is represented in excess K: the represented value is
  the written unsigned value minus K, where K is half the maximum for the
  corresponding unsigned argument.
===========================================================================*/


enum OpMode {iABC, iABx, iAsBx, iAx, isJ};  /* basic instruction formats */


/*
** size and position of opcode arguments.
*/
#define SIZE_C		8
#define SIZE_Cx		(SIZE_C + 1)
#define SIZE_B		8
#define SIZE_Bx		(SIZE_Cx + SIZE_B)
#define SIZE_A		8
#define SIZE_Ax		(SIZE_Cx + SIZE_B + SIZE_A)
#define SIZE_sJ		(SIZE_C + SIZE_B + SIZE_A)


#define SIZE_OP		7

#define POS_OP		0
#define POS_A		(POS_OP + SIZE_OP)
#define POS_B		(POS_A + SIZE_A)
#define POS_C		(POS_B + SIZE_B)
#define POS_k		(POS_C + SIZE_C)
#define POS_Bx		POS_B
#define POS_Ax		POS_A
#define POS_sJ		POS_A


/*
** limits for opcode arguments.
** we use (signed) int to manipulate most arguments,
** so they must fit in LUAI_BITSINT-1 bits (-1 for sign)
*/
#if SIZE_Bx < LUAI_BITSINT-1
#define MAXARG_Bx	((1<<SIZE_Bx)-1)
#else
#define MAXARG_Bx	MAX_INT
#endif

#define OFFSET_sBx	(MAXARG_Bx>>1)         /* 'sBx' is signed */


#if SIZE_Ax < LUAI_BITSINT-1
#define MAXARG_Ax	((1<<SIZE_Ax)-1)
#else
#define MAXARG_Ax	MAX_INT
#endif

#if SIZE_sJ < LUAI_BITSINT-1
#define MAXARG_sJ	((1 << SIZE_sJ) - 1)
#else
#define MAXARG_sJ	MAX_INT
#endif

#define OFFSET_sJ	(MAXARG_sJ >> 1)


#define MAXARG_A	((1<<SIZE_A)-1)
#define MAXARG_B	((1<<SIZE_B)-1)
#define MAXARG_C	((1<<SIZE_C)-1)
#define OFFSET_sC	(MAXARG_C >> 1)
#define MAXARG_Cx	((1<<(SIZE_C + 1))-1)


/* creates a mask with 'n' 1 bits at position 'p' */
#define MASK1(n,p)	((~((~(Instruction)0)<<(n)))<<(p))

/* creates a mask with 'n' 0 bits at position 'p' */
#define MASK0(n,p)	(~MASK1(n,p))

/*
** the following macros help to manipulate instructions
*/

#define GET_OPCODE(i)	(cast(OpCode, ((i)>>POS_OP) & MASK1(SIZE_OP,0)))
#define SET_OPCODE(i,o)	((i) = (((i)&MASK0(SIZE_OP,POS_OP)) | \
		((cast(Instruction, o)<<POS_OP)&MASK1(SIZE_OP,POS_OP))))

#define checkopm(i,m)	(getOpMode(GET_OPCODE(i)) == m)


#define getarg(i,pos,size)	(cast(int, ((i)>>(pos)) & MASK1(size,0)))
#define setarg(i,v,pos,size)	((i) = (((i)&MASK0(size,pos)) | \
                ((cast(Instruction, v)<<pos)&MASK1(size,pos))))

#define GETARG_A(i)	getarg(i, POS_A, SIZE_A)
#define SETARG_A(i,v)	setarg(i, v, POS_A, SIZE_A)

#define GETARG_B(i)	check_exp(checkopm(i, iABC), getarg(i, POS_B, SIZE_B))
#define GETARG_sB(i)	(GETARG_B(i) - OFFSET_sC)
#define SETARG_B(i,v)	setarg(i, v, POS_B, SIZE_B)

#define GETARG_C(i)	check_exp(checkopm(i, iABC), getarg(i, POS_C, SIZE_C))
#define GETARG_sC(i)	(GETARG_C(i) - OFFSET_sC)
#define SETARG_C(i,v)	setarg(i, v, POS_C, SIZE_C)

#define TESTARG_k(i)	(cast(int, ((i) & (1u << POS_k))))
#define GETARG_k(i)	getarg(i, POS_k, 1)
#define SETARG_k(i,v)	setarg(i, v, POS_k, 1)

#define GETARG_Bx(i)	check_exp(checkopm(i, iABx), getarg(i, POS_Bx, SIZE_Bx))
#define SETARG_Bx(i,v)	setarg(i, v, POS_Bx, SIZE_Bx)

#define GETARG_Ax(i)	check_exp(checkopm(i, iAx), getarg(i, POS_Ax, SIZE_Ax))
#define SETARG_Ax(i,v)	setarg(i, v, POS_Ax, SIZE_Ax)

#define GETARG_sBx(i)  \
	check_exp(checkopm(i, iAsBx), getarg(i, POS_Bx, SIZE_Bx) - OFFSET_sBx)
#define SETARG_sBx(i,b)	SETARG_Bx((i),cast(unsigned int, (b)+OFFSET_sBx))

#define GETARG_sJ(i)  \
	check_exp(checkopm(i, isJ), getarg(i, POS_sJ, SIZE_sJ) - OFFSET_sJ)
#define SETARG_sJ(i,j) \
	setarg(i, cast(unsigned int, (j)+OFFSET_sJ), POS_sJ, SIZE_sJ)


#define CREATE_ABCk(o,a,b,c,k)	((cast(Instruction, o)<<POS_OP) \
			| (cast(Instruction, a)<<POS_A) \
			| (cast(Instruction, b)<<POS_B) \
			| (cast(Instruction, c)<<POS_C) \
			| (cast(Instruction, k)<<POS_k))

#define CREATE_ABx(o,a,bc)	((cast(Instruction, o)<<POS_OP) \
			| (cast(Instruction, a)<<POS_A) \
			| (cast(Instruction, bc)<<POS_Bx))

#define CREATE_Ax(o,a)		((cast(Instruction, o)<<POS_OP) \
			| (cast(Instruction, a)<<POS_Ax))

#define CREATE_sJ(o,j,k)	((cast(Instruction, o) << POS_OP) \
			| (cast(Instruction, j) << POS_sJ) \
			| (cast(Instruction, k) << POS_k))


#if !defined(MAXINDEXRK)  /* (for debugging only) */
#define MAXINDEXRK	MAXARG_B
#endif


/*
** invalid register that fits in 8 bits
*/
#define NO_REG		MAXARG_A


/*
** R(x) - register
** K(x) - constant (in constant table)
** RK(x) == if k(i) then K(x) else R(x)
*/


/*
** grep "ORDER OP" if you change these enums
*/

typedef enum {
/*----------------------------------------------------------------------
name		args	description
------------------------------------------------------------------------*/
OP_MOVE,/*	A B	R(A) := R(B)					*/
OP_LOADI,/*	A sBx	R(A) := sBx					*/
OP_LOADF,/*	A sBx	R(A) := (lua_Number)sBx				*/
OP_LOADK,/*	A Bx	R(A) := K(Bx)					*/
OP_LOADKX,/*	A 	R(A) := K(extra arg)				*/
OP_LOADBOOL,/*	A B C	R(A) := (Bool)B; if (C) pc++			*/
OP_LOADNIL,/*	A B	R(A), R(A+1), ..., R(A+B) := nil		*/
OP_GETUPVAL,/*	A B	R(A) := UpValue[B]				*/
OP_SETUPVAL,/*	A B	UpValue[B] := R(A)				*/

OP_GETTABUP,/*	A B C	R(A) := UpValue[B][K(C):string]			*/
OP_GETTABLE,/*	A B C	R(A) := R(B)[R(C)]				*/
OP_GETI,/*	A B C	R(A) := R(B)[C]					*/
OP_GETFIELD,/*	A B C	R(A) := R(B)[K(C):string]			*/

OP_SETTABUP,/*	A B C	UpValue[A][K(B):string] := RK(C)		*/
OP_SETTABLE,/*	A B C	R(A)[R(B)] := RK(C)				*/
OP_SETI,/*	A B C	R(A)[B] := RK(C)				*/
OP_SETFIELD,/*	A B C	R(A)[K(B):string] := RK(C)			*/

OP_NEWTABLE,/*	A B C	R(A) := {} (size = B,C)				*/

OP_SELF,/*	A B C	R(A+1) := R(B); R(A) := R(B)[RK(C):string]	*/

OP_ADDI,/*	A B sC	R(A) := R(B) + C				*/
OP_SUBI,/*	A B sC	R(A) := R(B) - C				*/
OP_MULI,/*	A B sC	R(A) := R(B) * C				*/
OP_MODI,/*	A B sC	R(A) := R(B) % C				*/
OP_POWI,/*	A B sC	R(A) := R(B) ^ C				*/
OP_DIVI,/*	A B sC	R(A) := R(B) / C				*/
OP_IDIVI,/*	A B sC	R(A) := R(B) // C				*/

OP_BANDK,/*	A B C	R(A) := R(B) & K(C):integer			*/
OP_BORK,/*	A B C	R(A) := R(B) | K(C):integer			*/
OP_BXORK,/*	A B C	R(A) := R(B) ~ K(C):integer			*/

OP_SHRI,/*	A B C	R(A) := R(B) >> C				*/
OP_SHLI,/*	A B C	R(A) := C << R(B)				*/

OP_ADD,/*	A B C	R(A) := R(B) + R(C)				*/
OP_SUB,/*	A B C	R(A) := R(B) - R(C)				*/
OP_MUL,/*	A B C	R(A) := R(B) * R(C)				*/
OP_MOD,/*	A B C	R(A) := R(B) % R(C)				*/
OP_POW,/*	A B C	R(A) := R(B) ^ R(C)				*/
OP_DIV,/*	A B C	R(A) := R(B) / R(C)				*/
OP_IDIV,/*	A B C	R(A) := R(B) // R(C)				*/
OP_BAND,/*	A B C	R(A) := R(B) & R(C)				*/
OP_BOR,/*	A B C	R(A) := R(B) | R(C)				*/
OP_BXOR,/*	A B C	R(A) := R(B) ~ R(C)				*/
OP_SHL,/*	A B C	R(A) := R(B) << R(C)				*/
OP_SHR,/*	A B C	R(A) := R(B) >> R(C)				*/
OP_UNM,/*	A B	R(A) := -R(B)					*/
OP_BNOT,/*	A B	R(A) := ~R(B)					*/
OP_NOT,/*	A B	R(A) := not R(B)				*/
OP_LEN,/*	A B	R(A) := length of R(B)				*/

OP_CONCAT,/*	A B C	R(A) := R(B).. ... ..R(C)			*/

OP_CLOSE,/*	A	close all upvalues >= R(A)			*/
OP_JMP,/*	k sJ	pc += sJ  (k is used in code generation)	*/
OP_EQ,/*	A B	if ((R(A) == R(B)) ~= k) then pc++		*/
OP_LT,/*	A B	if ((R(A) <  R(B)) ~= k) then pc++		*/
OP_LE,/*	A B	if ((R(A) <= R(B)) ~= k) then pc++		*/

OP_EQK,/*	A B	if ((R(A) == K(B)) ~= k) then pc++		*/
OP_EQI,/*	A sB	if ((R(A) == sB) ~= k) then pc++		*/
OP_LTI,/*	A sB	if ((R(A) < sB) ~= k) then pc++			*/
OP_LEI,/*	A sB	if ((R(A) <= sB) ~= k) then pc++		*/

OP_TEST,/*	A 	if (not R(A) == k) then pc++			*/
OP_TESTSET,/*	A B	if (not R(B) == k) then R(A) := R(B) else pc++	*/

OP_CALL,/*	A B C	R(A), ... ,R(A+C-2) := R(A)(R(A+1), ... ,R(A+B-1)) */
OP_TAILCALL,/*	A B C	return R(A)(R(A+1), ... ,R(A+B-1))		*/

OP_RETURN,/*	A B	return R(A), ... ,R(A+B-2)	(see note)	*/
OP_RETURN0,/*	  	return 						*/
OP_RETURN1,/*	A 	return R(A)					*/

OP_FORLOOP1,/*	A Bx	R(A)++;
			if R(A) <= R(A+1) then { pc-=Bx; R(A+3)=R(A) }	*/
OP_FORPREP1,/*	A Bx	R(A)--; pc+=Bx					*/

OP_FORLOOP,/*	A Bx	R(A)+=R(A+2);
			if R(A) <?= R(A+1) then { pc-=Bx; R(A+3)=R(A) }	*/
OP_FORPREP,/*	A Bx	R(A)-=R(A+2); pc+=Bx				*/

OP_TFORCALL,/*	A C	R(A+3), ... ,R(A+2+C) := R(A)(R(A+1), R(A+2));	*/
OP_TFORLOOP,/*	A Bx	if R(A+1) ~= nil then { R(A)=R(A+1); pc -= Bx }	*/

OP_SETLIST,/*	A B C	R(A)[(C-1)*FPF+i] := R(A+i), 1 <= i <= B	*/

OP_CLOSURE,/*	A Bx	R(A) := closure(KPROTO[Bx])			*/

OP_VARARG,/*	A B C	R(A), R(A+1), ..., R(A+C-2) = vararg(B)		*/

OP_EXTRAARG/*	Ax	extra (larger) argument for previous opcode	*/
} OpCode;


#define NUM_OPCODES	(cast(int, OP_EXTRAARG) + 1)



/*===========================================================================
  Notes:
  (*) In OP_CALL, if (B == 0) then B = top. If (C == 0), then 'top' is
  set to last_result+1, so next open instruction (OP_CALL, OP_RETURN,
  OP_SETLIST) may use 'top'.

  (*) In OP_VARARG, if (C == 0) then use actual number of varargs and
  set top (like in OP_CALL with C == 0). B is the vararg parameter.

  (*) In OP_RETURN, if (B == 0) then return up to 'top'.

  (*) In OP_SETLIST, if (B == 0) then real B = 'top'; if (C == 0) then
  next 'instruction' is EXTRAARG(real C).

  (*) In OP_LOADKX, the next 'instruction' is always EXTRAARG.

  (*) For comparisons, k specifies what condition the test should accept
  (true or false).

  (*) For OP_LTI/OP_LEI, C indicates that the transformations
  (A<B) => (!(B<=A)) or (A<=B) => (!(B<A)) were used to put the constant
  operator on the right side. (Non-total orders with NaN or metamethods
  use this indication to correct their behavior.)

  (*) All 'skips' (pc++) assume that next instruction is a jump.

  (*) In instructions ending a function (OP_RETURN*, OP_TAILCALL), k
  specifies that the function builds upvalues, which may need to be
  closed.

===========================================================================*/


/*
** masks for instruction properties. The format is:
** bits 0-2: op mode
** bit 3: instruction set register A
** bit 4: operator is a test (next instruction must be a jump)
** bit 5: instruction sets 'L->top' for next instruction (when C == 0)
** bit 6: instruction uses 'L->top' set by previous instruction (when B == 0)
*/

LUAI_DDEC const lu_byte luaP_opmodes[NUM_OPCODES];

#define getOpMode(m)	(cast(enum OpMode, luaP_opmodes[m] & 7))
#define testAMode(m)	(luaP_opmodes[m] & (1 << 3))
#define testTMode(m)	(luaP_opmodes[m] & (1 << 4))
#define testOTMode(m)	(luaP_opmodes[m] & (1 << 5))
#define testITMode(m)	(luaP_opmodes[m] & (1 << 6))

/* "out top" (set top for next instruction) */
#define isOT(i)		(testOTMode(GET_OPCODE(i)) && GETARG_C(i) == 0)

/* "in top" (uses top from previous instruction) */
#define isIT(i)		(testITMode(GET_OPCODE(i)) && GETARG_B(i) == 0)

#define opmode(ot,it,t,a,m) (((ot)<<5) | ((it)<<6) | ((t)<<4) | ((a)<<3) | (m))


LUAI_DDEC const char *const luaP_opnames[NUM_OPCODES+1];  /* opcode names */


/* number of list items to accumulate before a SETLIST instruction */
#define LFIELDS_PER_FLUSH	50


#endif
