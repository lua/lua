/*
** $Id: lopcodes.h,v 1.18 1998/06/25 14:37:00 roberto Exp $
** Opcodes for Lua virtual machine
** See Copyright Notice in lua.h
*/

#ifndef lopcodes_h
#define lopcodes_h


/*
** NOTICE: variants of the same opcode must be consecutive: First, those
** with byte parameter, then with built-in parameters, and last with
** word parameter.
*/


typedef enum {
/* name          parm    before          after           side effect
-----------------------------------------------------------------------------*/
ENDCODE,/*	-	-		-  */

PUSHNIL,/*	b	-		nil_0...nil_b  */
PUSHNIL0,/*	-	-		nil  */

PUSHNUMBER,/*	b	-		(float)b  */
PUSHNUMBER0,/*	-	-		0.0  */
PUSHNUMBER1,/*	-	-		1.0  */
PUSHNUMBER2,/*	-	-		2.0  */
PUSHNUMBERW,/*	w	-		(float)w  */

PUSHCONSTANT,/*	b	-		CNST[b] */
PUSHCONSTANT0,/*-	-		CNST[0] */
PUSHCONSTANT1,/*-	-		CNST[1] */
PUSHCONSTANT2,/*-	-		CNST[2] */
PUSHCONSTANT3,/*-	-		CNST[3] */
PUSHCONSTANT4,/*-	-		CNST[4] */
PUSHCONSTANT5,/*-	-		CNST[5] */
PUSHCONSTANT6,/*-	-		CNST[6] */
PUSHCONSTANT7,/*-	-		CNST[7] */
PUSHCONSTANTW,/*w	-		CNST[w] */

PUSHUPVALUE,/*	b	-		Closure[b] */
PUSHUPVALUE0,/*	-	-		Closure[0] */
PUSHUPVALUE1,/*	-	-		Closure[1] */

PUSHLOCAL,/*	b	-		LOC[b]  */
PUSHLOCAL0,/*	-	-		LOC[0]  */
PUSHLOCAL1,/*	-	-		LOC[1]  */
PUSHLOCAL2,/*	-	-		LOC[2]  */
PUSHLOCAL3,/*	-	-		LOC[3]  */
PUSHLOCAL4,/*	-	-		LOC[4]  */
PUSHLOCAL5,/*	-	-		LOC[5]  */
PUSHLOCAL6,/*	-	-		LOC[6]  */
PUSHLOCAL7,/*	-	-		LOC[7]  */

GETGLOBAL,/*	b 	-		VAR[CNST[b]]  */
GETGLOBAL0,/*	-	-		VAR[CNST[0]]  */
GETGLOBAL1,/*	-	-		VAR[CNST[1]]  */
GETGLOBAL2,/*	-	-		VAR[CNST[2]]  */
GETGLOBAL3,/*	-	-		VAR[CNST[3]]  */
GETGLOBAL4,/*	-	-		VAR[CNST[4]]  */
GETGLOBAL5,/*	-	-		VAR[CNST[5]]  */
GETGLOBAL6,/*	-	-		VAR[CNST[6]]  */
GETGLOBAL7,/*	-	-		VAR[CNST[7]]  */
GETGLOBALW,/*	w	-		VAR[CNST[w]]  */

GETTABLE,/*	-	i t		t[i]  */

GETDOTTED,/*	b	t		t[CNST[b]]  */
GETDOTTED0,/*	-	t		t[CNST[0]]  */
GETDOTTED1,/*	-	t		t[CNST[1]]  */
GETDOTTED2,/*	-	t		t[CNST[2]]  */
GETDOTTED3,/*	-	t		t[CNST[3]]  */
GETDOTTED4,/*	-	t		t[CNST[4]]  */
GETDOTTED5,/*	-	t		t[CNST[5]]  */
GETDOTTED6,/*	-	t		t[CNST[6]]  */
GETDOTTED7,/*	-	t		t[CNST[7]]  */
GETDOTTEDW,/*	w	t		t[CNST[w]]  */

PUSHSELF,/*	b	t		t t[CNST[b]]  */
PUSHSELF0,/*	-	t		t t[CNST[0]]  */
PUSHSELF1,/*	-	t		t t[CNST[1]]  */
PUSHSELF2,/*	-	t		t t[CNST[2]]  */
PUSHSELF3,/*	-	t		t t[CNST[3]]  */
PUSHSELF4,/*	-	t		t t[CNST[4]]  */
PUSHSELF5,/*	-	t		t t[CNST[5]]  */
PUSHSELF6,/*	-	t		t t[CNST[6]]  */
PUSHSELF7,/*	-	t		t t[CNST[7]]  */
PUSHSELFW,/*	w	t		t t[CNST[w]]  */

CREATEARRAY,/*	b	-		newarray(size = b)  */
CREATEARRAY0,/*	-	-		newarray(size = 0)  */
CREATEARRAY1,/*	-	-		newarray(size = 1)  */
CREATEARRAYW,/*	w	-		newarray(size = w)  */

SETLOCAL,/*	b	x		-		LOC[b]=x  */
SETLOCAL0,/*	-	x		-		LOC[0]=x  */
SETLOCAL1,/*	-	x		-		LOC[1]=x  */
SETLOCAL2,/*	-	x		-		LOC[2]=x  */
SETLOCAL3,/*	-	x		-		LOC[3]=x  */
SETLOCAL4,/*	-	x		-		LOC[4]=x  */
SETLOCAL5,/*	-	x		-		LOC[5]=x  */
SETLOCAL6,/*	-	x		-		LOC[6]=x  */
SETLOCAL7,/*	-	x		-		LOC[7]=x  */

SETGLOBAL,/*	b	x		-		VAR[CNST[b]]=x  */
SETGLOBAL0,/*	-	x		-		VAR[CNST[0]]=x  */
SETGLOBAL1,/*	-	x		-		VAR[CNST[1]]=x  */
SETGLOBAL2,/*	-	x		-		VAR[CNST[2]]=x  */
SETGLOBAL3,/*	-	x		-		VAR[CNST[3]]=x  */
SETGLOBAL4,/*	-	x		-		VAR[CNST[4]]=x  */
SETGLOBAL5,/*	-	x		-		VAR[CNST[5]]=x  */
SETGLOBAL6,/*	-	x		-		VAR[CNST[6]]=x  */
SETGLOBAL7,/*	-	x		-		VAR[CNST[7]]=x  */
SETGLOBALW,/*	w	x		-		VAR[CNST[w]]=x  */

SETTABLE0,/*	-	v i t		-		t[i]=v  */

SETTABLE,/*	b	v a_b...a_1 i t	a_b...a_1 i t	t[i]=v  */

SETLIST,/*	b c	v_c...v_1 t	-		t[i+b*FPF]=v_i  */
SETLIST0,/*	b	v_b...v_1 t	-		t[i]=v_i  */
SETLISTW,/*	w c	v_c...v_1 t	-		t[i+w*FPF]=v_i  */

SETMAP,/*	b	v_b k_b ...v_0 k_0 t	t	t[k_i]=v_i  */
SETMAP0,/*	-	v_0 k_0 t		t	t[k_0]=v_0  */

EQOP,/*		-	y x		(x==y)? 1 : nil  */
NEQOP,/*	-	y x		(x~=y)? 1 : nil  */
LTOP,/*		-	y x		(x<y)? 1 : nil  */
LEOP,/*		-	y x		(x<y)? 1 : nil  */
GTOP,/*		-	y x		(x>y)? 1 : nil  */
GEOP,/*		-	y x		(x>=y)? 1 : nil  */
ADDOP,/*	-	y x		x+y  */
SUBOP,/*	-	y x		x-y  */
MULTOP,/*	-	y x		x*y  */
DIVOP,/*	-	y x		x/y  */
POWOP,/*	-	y x		x^y  */
CONCOP,/*	-	y x		x..y  */
MINUSOP,/*	-	x		-x  */
NOTOP,/*	-	x		(x==nil)? 1 : nil  */

ONTJMP,/*	b	x		(x!=nil)? x : -	(x!=nil)? PC+=b  */
ONTJMPW,/*	w	x		(x!=nil)? x : -	(x!=nil)? PC+=w  */
ONFJMP,/*	b	x		(x==nil)? x : -	(x==nil)? PC+=b  */
ONFJMPW,/*	w	x		(x==nil)? x : -	(x==nil)? PC+=w  */
JMP,/*		b	-		-		PC+=b  */
JMPW,/*		w	-		-		PC+=w  */
IFFJMP,/*	b	x		-		(x==nil)? PC+=b  */
IFFJMPW,/*	w	x		-		(x==nil)? PC+=w  */
IFTUPJMP,/*	b	x		-		(x!=nil)? PC-=b  */
IFTUPJMPW,/*	w	x		-		(x!=nil)? PC-=w  */
IFFUPJMP,/*	b	x		-		(x==nil)? PC-=b  */
IFFUPJMPW,/*	w	x		-		(x==nil)? PC-=w  */

CLOSURE,/*	b c	v_c...v_1	closure(CNST[b], v_c...v_1) */
CLOSUREW,/*	w c	v_b...v_1	closure(CNST[w], v_c...v_1) */

CALLFUNC,/*	b c	v_c...v_1 f	r_b...r_1	f(v1,...,v_c)  */
CALLFUNC0,/*	b	v_b...v_1 f	-		f(v1,...,v_b)  */
CALLFUNC1,/*	b	v_b...v_1 f	r_1		f(v1,...,v_b)  */

RETCODE,/*	b	-		-  */

SETLINE,/*	b	-		-		LINE=b  */
SETLINEW,/*	w	-		-		LINE=w  */

POP,/*		b	-		-		TOP-=(b+1)  */
POP0,/*		-	-		-		TOP-=1  */
POP1/*		-	-		-		TOP-=2  */

} OpCode;


#define RFIELDS_PER_FLUSH 32	/* records (SETMAP) */
#define LFIELDS_PER_FLUSH 64    /* lists (SETLIST) */

#define ZEROVARARG	64

#endif
