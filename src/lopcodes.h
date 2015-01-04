/*
** $Id: lopcodes.h,v 1.68 2000/10/24 16:05:59 roberto Exp $
** Opcodes for Lua virtual machine
** See Copyright Notice in lua.h
*/

#ifndef lopcodes_h
#define lopcodes_h

#include "llimits.h"


/*===========================================================================
  We assume that instructions are unsigned numbers.
  All instructions have an opcode in the first 6 bits. Moreover,
  an instruction can have 0, 1, or 2 arguments. Instructions can
  have the following types:
  type 0: no arguments
  type 1: 1 unsigned argument in the higher bits (called `U')
  type 2: 1 signed argument in the higher bits          (`S')
  type 3: 1st unsigned argument in the higher bits      (`A')
          2nd unsigned argument in the middle bits      (`B')

  A signed argument is represented in excess K; that is, the number
  value is the unsigned value minus K. K is exactly the maximum value
  for that argument (so that -max is represented by 0, and +max is
  represented by 2*max), which is half the maximum for the corresponding
  unsigned argument.

  The size of each argument is defined in `llimits.h'. The usual is an
  instruction with 32 bits, U arguments with 26 bits (32-6), B arguments
  with 9 bits, and A arguments with 17 bits (32-6-9). For small
  installations, the instruction size can be 16, so U has 10 bits,
  and A and B have 5 bits each.
===========================================================================*/




/* creates a mask with `n' 1 bits at position `p' */
#define MASK1(n,p)	((~((~(Instruction)0)<<n))<<p)

/* creates a mask with `n' 0 bits at position `p' */
#define MASK0(n,p)	(~MASK1(n,p))

/*
** the following macros help to manipulate instructions
*/

#define CREATE_0(o)	 ((Instruction)(o))
#define GET_OPCODE(i)	((OpCode)((i)&MASK1(SIZE_OP,0)))
#define SET_OPCODE(i,o)	((i) = (((i)&MASK0(SIZE_OP,0)) | (Instruction)(o)))

#define CREATE_U(o,u)	 ((Instruction)(o) | ((Instruction)(u)<<POS_U))
#define GETARG_U(i)	((int)((i)>>POS_U))
#define SETARG_U(i,u)	((i) = (((i)&MASK0(SIZE_U,POS_U)) | \
                               ((Instruction)(u)<<POS_U)))

#define CREATE_S(o,s)	CREATE_U((o),(s)+MAXARG_S)
#define GETARG_S(i)	(GETARG_U(i)-MAXARG_S)
#define SETARG_S(i,s)	SETARG_U((i),(s)+MAXARG_S)


#define CREATE_AB(o,a,b) ((Instruction)(o) | ((Instruction)(a)<<POS_A) \
                                           |  ((Instruction)(b)<<POS_B))
#define GETARG_A(i)	((int)((i)>>POS_A))
#define SETARG_A(i,a)	((i) = (((i)&MASK0(SIZE_A,POS_A)) | \
                               ((Instruction)(a)<<POS_A)))
#define GETARG_B(i)	((int)(((i)>>POS_B) & MASK1(SIZE_B,0)))
#define SETARG_B(i,b)	((i) = (((i)&MASK0(SIZE_B,POS_B)) | \
                               ((Instruction)(b)<<POS_B)))


/*
** K = U argument used as index to `kstr'
** J = S argument used as jump offset (relative to pc of next instruction)
** L = unsigned argument used as index of local variable
** N = U argument used as index to `knum'
*/

typedef enum {
/*----------------------------------------------------------------------
name		args	stack before	stack after	side effects
------------------------------------------------------------------------*/
OP_END,/*	-	-		(return)	no results	*/
OP_RETURN,/*	U	v_n-v_x(at u)	(return)	returns v_x-v_n	*/

OP_CALL,/*	A B	v_n-v_1 f(at a)	r_b-r_1		f(v1,...,v_n)	*/
OP_TAILCALL,/*	A B	v_n-v_1 f(at a)	(return)	f(v1,...,v_n)	*/

OP_PUSHNIL,/*	U	-		nil_1-nil_u			*/
OP_POP,/*	U	a_u-a_1		-				*/

OP_PUSHINT,/*	S	-		(Number)s			*/
OP_PUSHSTRING,/* K	-		KSTR[k]				*/
OP_PUSHNUM,/*	N	-		KNUM[n]				*/
OP_PUSHNEGNUM,/* N	-		-KNUM[n]			*/

OP_PUSHUPVALUE,/* U	-		Closure[u]			*/

OP_GETLOCAL,/*	L	-		LOC[l]				*/
OP_GETGLOBAL,/*	K	-		VAR[KSTR[k]]			*/

OP_GETTABLE,/*	-	i t		t[i]				*/
OP_GETDOTTED,/*	K	t		t[KSTR[k]]			*/
OP_GETINDEXED,/* L	t		t[LOC[l]]			*/
OP_PUSHSELF,/*	K	t		t t[KSTR[k]]			*/

OP_CREATETABLE,/* U	-		newarray(size = u)		*/

OP_SETLOCAL,/*	L	x		-		LOC[l]=x	*/
OP_SETGLOBAL,/*	K	x		-		VAR[KSTR[k]]=x	*/
OP_SETTABLE,/*	A B	v a_a-a_1 i t	(pops b values)	t[i]=v		*/

OP_SETLIST,/*	A B	v_b-v_1 t	t		t[i+a*FPF]=v_i	*/
OP_SETMAP,/*	U	v_u k_u - v_1 k_1 t	t	t[k_i]=v_i	*/

OP_ADD,/*	-	y x		x+y				*/
OP_ADDI,/*	S	x		x+s				*/
OP_SUB,/*	-	y x		x-y				*/
OP_MULT,/*	-	y x		x*y				*/
OP_DIV,/*	-	y x		x/y				*/
OP_POW,/*	-	y x		x^y				*/
OP_CONCAT,/*	U	v_u-v_1		v1..-..v_u			*/
OP_MINUS,/*	-	x		-x				*/
OP_NOT,/*	-	x		(x==nil)? 1 : nil		*/

OP_JMPNE,/*	J	y x		-		(x~=y)? PC+=s	*/
OP_JMPEQ,/*	J	y x		-		(x==y)? PC+=s	*/
OP_JMPLT,/*	J	y x		-		(x<y)? PC+=s	*/
OP_JMPLE,/*	J	y x		-		(x<y)? PC+=s	*/
OP_JMPGT,/*	J	y x		-		(x>y)? PC+=s	*/
OP_JMPGE,/*	J	y x		-		(x>=y)? PC+=s	*/

OP_JMPT,/*	J	x		-		(x~=nil)? PC+=s	*/
OP_JMPF,/*	J	x		-		(x==nil)? PC+=s	*/
OP_JMPONT,/*	J	x		(x~=nil)? x : -	(x~=nil)? PC+=s	*/
OP_JMPONF,/*	J	x		(x==nil)? x : -	(x==nil)? PC+=s	*/
OP_JMP,/*	J	-		-		PC+=s		*/

OP_PUSHNILJMP,/* -	-		nil		PC++;		*/

OP_FORPREP,/*	J							*/
OP_FORLOOP,/*	J							*/

OP_LFORPREP,/*	J							*/
OP_LFORLOOP,/*	J							*/

OP_CLOSURE/*	A B	v_b-v_1		closure(KPROTO[a], v_1-v_b)	*/

} OpCode;

#define NUM_OPCODES	((int)OP_CLOSURE+1)


#define ISJUMP(o)	(OP_JMPNE <= (o) && (o) <= OP_JMP)



/* special code to fit a LUA_MULTRET inside an argB */
#define MULT_RET        255	/* (<=MAXARG_B) */
#if MULT_RET>MAXARG_B
#undef MULT_RET
#define MULT_RET	MAXARG_B
#endif


#endif
