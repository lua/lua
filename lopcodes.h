/*
** $Id: lopcodes.h,v 1.4 1997/09/22 20:53:20 roberto Exp roberto $
** Opcodes for Lua virtual machine
** See Copyright Notice in lua.h
*/

#ifndef lopcodes_h
#define lopcodes_h


/*
** NOTICE: variants of the same opcode (like PUSH0, PUSHBYTE, PUSHWORD) must
** be consecutive: First, those with built-in parameters, then with byte
** parameter and last with word parameter.
*/


typedef enum {
/* name          parm    before          after           side effect
-----------------------------------------------------------------------------*/
ENDCODE,

PUSHNIL,/*		-		nil  */
PUSHNILS,/*	b	-		nil_1...nil_b  */

PUSH0,/*		-		0.0  */
PUSH1,/*		-		1.0  */
PUSH2,/*		-		2.0  */
PUSHBYTE,/*	b	-		(float)b  */
PUSHWORD,/*	w	-		(float)w  */

PUSHCONSTANT0,/*	-		CNST[0] */
PUSHCONSTANT1,/*	-		CNST[1] */
PUSHCONSTANT2,/*	-		CNST[2] */
PUSHCONSTANT3,/*	-		CNST[3] */
PUSHCONSTANT4,/*	-		CNST[4] */
PUSHCONSTANT5,/*	-		CNST[5] */
PUSHCONSTANT6,/*	-		CNST[6] */
PUSHCONSTANT7,/*	-		CNST[7] */
PUSHCONSTANT8,/*	-		CNST[8] */
PUSHCONSTANT9,/*	-		CNST[9] */
PUSHCONSTANTB,/*b	-		CNST[b] */
PUSHCONSTANT,/* w	-		CNST[w] */

PUSHUPVALUE0,
PUSHUPVALUE1,
PUSHUPVALUE,/*	b	-		Closure[b] */

PUSHLOCAL0,/*		-		LOC[0]  */
PUSHLOCAL1,/*		-		LOC[1]  */
PUSHLOCAL2,/*		-		LOC[2]  */
PUSHLOCAL3,/*		-		LOC[3]  */
PUSHLOCAL4,/*		-		LOC[4]  */
PUSHLOCAL5,/*		-		LOC[5]  */
PUSHLOCAL6,/*		-		LOC[6]  */
PUSHLOCAL7,/*		-		LOC[7]  */
PUSHLOCAL8,/*		-		LOC[8]  */
PUSHLOCAL9,/*		-		LOC[9]  */
PUSHLOCAL,/*	b	-		LOC[b]  */

GETGLOBAL0,/*		-		VAR[CNST[0]]  */
GETGLOBAL1,/*		-		VAR[CNST[1]]  */
GETGLOBAL2,/*		-		VAR[CNST[2]]  */
GETGLOBAL3,/*		-		VAR[CNST[3]]  */
GETGLOBAL4,/*		-		VAR[CNST[4]]  */
GETGLOBAL5,/*		-		VAR[CNST[5]]  */
GETGLOBAL6,/*		-		VAR[CNST[6]]  */
GETGLOBAL7,/*		-		VAR[CNST[7]]  */
GETGLOBAL8,/*		-		VAR[CNST[8]]  */
GETGLOBAL9,/*		-		VAR[CNST[9]]  */
GETGLOBALB,/*	b 	-		VAR[CNST[b]]  */
GETGLOBAL,/*	w	-		VAR[CNST[w]]  */

GETTABLE,/*		i t		t[i]  */

PUSHSELFB,/*	b	t		t t[CNST[b]]  */
PUSHSELF,/*	w	t		t t[CNST[w]]  */

CREATEARRAY,/*	w	-		newarray(size = w)  */

SETLOCAL0,/*		x		-		LOC[0]=x  */
SETLOCAL1,/*		x		-		LOC[1]=x  */
SETLOCAL2,/*		x		-		LOC[2]=x  */
SETLOCAL3,/*		x		-		LOC[3]=x  */
SETLOCAL4,/*		x		-		LOC[4]=x  */
SETLOCAL5,/*		x		-		LOC[5]=x  */
SETLOCAL6,/*		x		-		LOC[6]=x  */
SETLOCAL7,/*		x		-		LOC[7]=x  */
SETLOCAL8,/*		x		-		LOC[8]=x  */
SETLOCAL9,/*		x		-		LOC[9]=x  */
SETLOCAL,/*	b	x		-		LOC[b]=x  */

SETGLOBALB,/*	b	x		-		VAR[CNST[b]]=x  */
SETGLOBAL,/*	w	x		-		VAR[CNST[w]]=x  */

SETTABLE0,/*		v i t		-		t[i]=v  */
SETTABLE,/*	b	v a_b...a_1 i t	a_b...a_1 i t	t[i]=v  */

SETLIST0,/*	b	v_b...v_1 t	-		t[i]=v_i  */
SETLIST,/*	b c	v_b...v_1 t	-		t[i+c*FPF]=v_i  */
SETMAP,/*	b	v_b k_b ...v_1 k_1 t	t	t[k_i]=v_i  */

EQOP,/*			y x		(x==y)? 1 : nil  */
NEQOP,/*		y x		(x~=y)? 1 : nil  */
LTOP,/*			y x		(x<y)? 1 : nil  */
LEOP,/*			y x		(x<y)? 1 : nil  */
GTOP,/*			y x		(x>y)? 1 : nil  */
GEOP,/*			y x		(x>=y)? 1 : nil  */
ADDOP,/*		y x		x+y  */
SUBOP,/*		y x		x-y  */
MULTOP,/*		y x		x*y  */
DIVOP,/*		y x		x/y  */
POWOP,/*		y x		x^y  */
CONCOP,/*		y x		x..y  */
MINUSOP,/*		x		-x  */
NOTOP,/*		x		(x==nil)? 1 : nil  */

/* NOTICE: all jumps are relative to the position following the opcode */
ONTJMP,/*	b	x		(x!=nil)? x : -	(x!=nil)? PC+=b  */
ONFJMP,/*	b	x		(x==nil)? x : -	(x==nil)? PC+=b  */
JMP,/*		w	-		-		PC+=w  */
UPJMPB,/*	b	-		-		PC-=b  */
UPJMP,/*	w	-		-		PC-=w  */
IFFJMP,/*	w	x		-		(x==nil)? PC+=w  */
IFFUPJMPB,/*	b	x		-		(x==nil)? PC-=b  */
IFFUPJMP,/*	w	x		-		(x==nil)? PC-=w  */

CLOSUREB,/*	b	v_1...v_n	c(CNST[b]) */
CLOSURE,/*	w	v_1...v_n	c(CNST[w]) */

CALLFUNC,/*	b c	v_b...v_1 f	r_c...r_1	f(v1,...,v_b)  */
RETCODE,/*	b	-		-  */
SETLINE,/*	w	-		-		LINE=w  */
POP1,/*			-		-		TOP-=1  */
POP2,/*			-		-		TOP-=2  */
POPS,/*		b	-		-		TOP-=b  */
ARGS,/*		b	-		-		TOP=BASE+b  */
VARARGS/*	b	v_x...v_1	{v_1...v_x;n=x}	TOP=BASE+b+1  */
} OpCode;


#define RFIELDS_PER_FLUSH 32	/* records (SETMAP) */
#define LFIELDS_PER_FLUSH 64    /* lists (SETLIST) */


#endif
