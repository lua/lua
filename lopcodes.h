/*
** $Id: lopcodes.h,v 1.44 2000/03/03 18:53:17 roberto Exp roberto $
** Opcodes for Lua virtual machine
** See Copyright Notice in lua.h
*/

#ifndef lopcodes_h
#define lopcodes_h



/*===========================================================================
  We assume that instructions are unsigned numbers with 4 bytes.
  All instructions have an opcode in the lower byte. Moreover,
  an instruction can have 0, 1, or 2 arguments. There are 4 types of
  Instructions:
  type 0: no arguments
  type 1: 1 unsigned argument in the higher 24 bits (called `U')
  type 2: 1 signed argument in the higher 24 bits          (`S')
  type 3: 1st unsigned argument in the higher 16 bits      (`A')
          2nd unsigned argument in the middle 8 bits       (`B')

  The signed argument is represented in excess 2^23; that is, the real value
  is the usigned value minus 2^23.
===========================================================================*/

#define EXCESS_S	(1<<23)		/* == 2^23 */

/*
** the following macros help to manipulate instructions
*/

#define MAXARG_U	((1<<24)-1)
#define MAXARG_S	((1<<23)-1)
#define MAXARG_A	((1<<16)-1)
#define MAXARG_B	((1<<8)-1)

#define GET_OPCODE(i)	((OpCode)((i)&0xFF))
#define GETARG_U(i)	((int)((i)>>8))
#define GETARG_S(i)	((int)((i)>>8)-EXCESS_S)
#define GETARG_A(i)	((int)((i)>>16))
#define GETARG_B(i)	((int)(((i)>>8) & 0xFF))

#define SET_OPCODE(i,o)	(((i)&0xFFFFFF00u) | (Instruction)(o))
#define SETARG_U(i,u)	(((i)&0x000000FFu) | ((Instruction)(u)<<8))
#define SETARG_S(i,s)	(((i)&0x000000FFu) | ((Instruction)((s)+EXCESS_S)<<8))
#define SETARG_A(i,a)	(((i)&0x0000FFFFu) | ((Instruction)(a)<<16))
#define SETARG_B(i,b)	(((i)&0xFFFF00FFu) | ((Instruction)(b)<<8))

#define CREATE_0(o)	 ((Instruction)(o))
#define CREATE_U(o,u)	 ((Instruction)(o) | (Instruction)(u)<<8)
#define CREATE_S(o,s)	 ((Instruction)(o) | ((Instruction)(s)+EXCESS_S)<<8)
#define CREATE_AB(o,a,b) ((Instruction)(o) | ((Instruction)(a)<<16) \
                                           |  ((Instruction)(b)<<8))


/*
** K = U argument used as index to `kstr'
** J = S argument used as jump offset (relative to pc of next instruction)
** L = U argument used as index of local variable
** N = U argument used as index to `knum'
*/

typedef enum {
/*----------------------------------------------------------------------
name		args	stack before	stack after	side effects
------------------------------------------------------------------------*/
ENDCODE,/*	-	-		(return)			*/
RETCODE,/*	U	-		(return)			*/

CALL,/*		A B	v_n-v_1 f(at a)	r_b-r_1		f(v1,...,v_n)	*/
TAILCALL,/*	A B	v_a-v_1 f	(return)	f(v1,...,v_a)	*/

PUSHNIL,/*	U	-		nil_1-nil_u			*/
POP,/*		U	a_u-a_1		-				*/

PUSHINT,/*	S	-		(real)s				*/
PUSHSTRING,/*	K	-		KSTR[k]				*/
PUSHNUM,/*	N	-		KNUM[u]				*/
PUSHNEGNUM,/*	N	-		-KNUM[u]			*/

PUSHUPVALUE,/*	U	-		Closure[u]			*/

PUSHLOCAL,/*	L	-		LOC[u]				*/
GETGLOBAL,/*	K	-		VAR[KSTR[k]]			*/

GETTABLE,/*	-	i t		t[i]				*/
GETDOTTED,/*	K	t		t[KSTR[k]]			*/
PUSHSELF,/*	K	t		t t[KSTR[k]]			*/

CREATETABLE,/*	U	-		newarray(size = u)		*/

SETLOCAL,/*	L	x		-		LOC[u]=x	*/
SETGLOBAL,/*	K	x		-		VAR[KSTR[k]]=x	*/
SETTABLEPOP,/*	-	v i t		-		t[i]=v		*/
SETTABLE,/*	U	v a_u-a_1 i t	 a_u-a_1 i t	t[i]=v		*/

SETLIST,/*	A B	v_b-v_0 t	t		t[i+a*FPF]=v_i	*/
SETMAP,/*	U	v_u k_u - v_0 k_0 t	t	t[k_i]=v_i	*/

NEQOP,/*	-	y x		(x~=y)? 1 : nil			*/
EQOP,/*		-	y x		(x==y)? 1 : nil			*/
LTOP,/*		-	y x		(x<y)? 1 : nil			*/
LEOP,/*		-	y x		(x<y)? 1 : nil			*/
GTOP,/*		-	y x		(x>y)? 1 : nil			*/
GEOP,/*		-	y x		(x>=y)? 1 : nil			*/

ADDOP,/*	-	y x		x+y				*/
ADDI,/*		S	x		x+s				*/
SUBOP,/*	-	y x		x-y				*/
MULTOP,/*	-	y x		x*y				*/
DIVOP,/*	-	y x		x/y				*/
POWOP,/*	-	y x		x^y				*/
CONCOP,/*	-	y x		x..y				*/
MINUSOP,/*	-	x		-x				*/
NOTOP,/*	-	x		(x==nil)? 1 : nil		*/

ONTJMP,/*	J	x		(x!=nil)? x : -	(x!=nil)? PC+=s	*/
ONFJMP,/*	J	x		(x==nil)? x : -	(x==nil)? PC+=s	*/
JMP,/*		J	-		-		PC+=s		*/
IFTJMP,/*	J	x		-		(x!=nil)? PC+=s	*/
IFFJMP,/*	J	x		-		(x==nil)? PC+=s	*/

CLOSURE,/*	A B	v_b-v_1		closure(CNST[a], v_b-v_1)	*/

SETLINE/*	U	-		-		LINE=u		*/

} OpCode;


#define RFIELDS_PER_FLUSH 32	/* records (SETMAP) */
#define LFIELDS_PER_FLUSH 64    /* FPF - lists (SETLIST) (<=MAXARG_B) */


#endif
