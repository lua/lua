/*
** $Id: lopcodes.h,v 1.19 1999/02/02 17:57:49 roberto Exp roberto $
** Opcodes for Lua virtual machine
** See Copyright Notice in lua.h
*/

#ifndef lopcodes_h
#define lopcodes_h


#define NUMOFFSET	100

/*
** NOTICE: variants of the same opcode must be consecutive: First, those
** with word parameter, then with byte parameter.
*/


typedef enum {
/* name          parm    before          after           side effect
-----------------------------------------------------------------------------*/
ENDCODE,/*	-	-		-  */
RETCODE,/*	b	-		-  */

PUSHNIL,/*	b	-		nil_0...nil_b  */
POP,/*		b	-		-		TOP-=(b+1)  */

PUSHNUMBERW,/*	w	-		(float)(w-NUMOFFSET)  */
PUSHNUMBER,/*	b	-		(float)(b-NUMOFFSET)  */

PUSHCONSTANTW,/*w	-		CNST[w] */
PUSHCONSTANT,/*	b	-		CNST[b] */

PUSHUPVALUE,/*	b	-		Closure[b] */

PUSHLOCAL,/*	b	-		LOC[b]  */

GETGLOBALW,/*	w	-		VAR[CNST[w]]  */
GETGLOBAL,/*	b 	-		VAR[CNST[b]]  */

GETTABLE,/*	-	i t		t[i]  */

GETDOTTEDW,/*	w	t		t[CNST[w]]  */
GETDOTTED,/*	b	t		t[CNST[b]]  */

PUSHSELFW,/*	w	t		t t[CNST[w]]  */
PUSHSELF,/*	b	t		t t[CNST[b]]  */

CREATEARRAYW,/*	w	-		newarray(size = w)  */
CREATEARRAY,/*	b	-		newarray(size = b)  */

SETLOCAL,/*	b	x		-		LOC[b]=x  */
SETLOCALDUP,/*	b	x		x		LOC[b]=x  */

SETGLOBALW,/*	w	x		-		VAR[CNST[w]]=x  */
SETGLOBAL,/*	b	x		-		VAR[CNST[b]]=x  */
SETGLOBALDUPW,/*w	x		x		VAR[CNST[w]]=x  */
SETGLOBALDUP,/*	b	x		x		VAR[CNST[b]]=x  */

SETTABLE0,/*	-	v i t		-		t[i]=v  */
SETTABLEDUP,/*	-	v i t		v		t[i]=v  */

SETTABLE,/*	b	v a_b...a_1 i t	a_b...a_1 i t	t[i]=v  */

SETLISTW,/*	w c	v_c...v_1 t	-		t[i+w*FPF]=v_i  */
SETLIST,/*	b c	v_c...v_1 t	-		t[i+b*FPF]=v_i  */

SETMAP,/*	b	v_b k_b ...v_0 k_0 t	t	t[k_i]=v_i  */

NEQOP,/*	-	y x		(x~=y)? 1 : nil  */
EQOP,/*		-	y x		(x==y)? 1 : nil  */
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

ONTJMPW,/*	w	x		(x!=nil)? x : -	(x!=nil)? PC+=w  */
ONTJMP,/*	b	x		(x!=nil)? x : -	(x!=nil)? PC+=b  */
ONFJMPW,/*	w	x		(x==nil)? x : -	(x==nil)? PC+=w  */
ONFJMP,/*	b	x		(x==nil)? x : -	(x==nil)? PC+=b  */
JMPW,/*		w	-		-		PC+=w  */
JMP,/*		b	-		-		PC+=b  */
IFFJMPW,/*	w	x		-		(x==nil)? PC+=w  */
IFFJMP,/*	b	x		-		(x==nil)? PC+=b  */
IFTUPJMPW,/*	w	x		-		(x!=nil)? PC-=w  */
IFTUPJMP,/*	b	x		-		(x!=nil)? PC-=b  */
IFFUPJMPW,/*	w	x		-		(x==nil)? PC-=w  */
IFFUPJMP,/*	b	x		-		(x==nil)? PC-=b  */

CLOSURE,/*	b c	v_c...v_1	closure(CNST[b], v_c...v_1) */

CALLFUNC,/*	b c	v_c...v_1 f	r_b...r_1	f(v1,...,v_c)  */

SETLINEW,/*	w	-		-		LINE=w  */
SETLINE /*	b	-		-		LINE=b  */

} OpCode;


#define RFIELDS_PER_FLUSH 32	/* records (SETMAP) */
#define LFIELDS_PER_FLUSH 64    /* lists (SETLIST) */

#define ZEROVARARG	64

#endif
