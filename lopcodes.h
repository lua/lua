/*
** $Id: lopcodes.h,v 1.18 1998/06/25 14:37:00 roberto Exp roberto $
** Opcodes for Lua virtual machine
** See Copyright Notice in lua.h
*/

#ifndef lopcodes_h
#define lopcodes_h


#define NUMOFFSET	100

/*
** NOTICE: variants of the same opcode must be consecutive: First, those
** with byte parameter, then with word parameter.
*/


typedef enum {
/* name          parm    before          after           side effect
-----------------------------------------------------------------------------*/
ENDCODE,/*	-	-		-  */

PUSHNIL,/*	b	-		nil_0...nil_b  */

PUSHNUMBER,/*	b	-		(float)(b-NUMOFFSET)  */
PUSHNUMBERW,/*	w	-		(float)(w-NUMOFFSET)  */

PUSHCONSTANT,/*	b	-		CNST[b] */
PUSHCONSTANTW,/*w	-		CNST[w] */

PUSHUPVALUE,/*	b	-		Closure[b] */

PUSHLOCAL,/*	b	-		LOC[b]  */

GETGLOBAL,/*	b 	-		VAR[CNST[b]]  */
GETGLOBALW,/*	w	-		VAR[CNST[w]]  */

GETTABLE,/*	-	i t		t[i]  */

GETDOTTED,/*	b	t		t[CNST[b]]  */
GETDOTTEDW,/*	w	t		t[CNST[w]]  */

PUSHSELF,/*	b	t		t t[CNST[b]]  */
PUSHSELFW,/*	w	t		t t[CNST[w]]  */

CREATEARRAY,/*	b	-		newarray(size = b)  */
CREATEARRAYW,/*	w	-		newarray(size = w)  */

SETLOCAL,/*	b	x		-		LOC[b]=x  */

SETGLOBAL,/*	b	x		-		VAR[CNST[b]]=x  */
SETGLOBALW,/*	w	x		-		VAR[CNST[w]]=x  */

SETTABLE0,/*	-	v i t		-		t[i]=v  */

SETTABLE,/*	b	v a_b...a_1 i t	a_b...a_1 i t	t[i]=v  */

SETLIST,/*	b c	v_c...v_1 t	-		t[i+b*FPF]=v_i  */
SETLISTW,/*	w c	v_c...v_1 t	-		t[i+w*FPF]=v_i  */

SETMAP,/*	b	v_b k_b ...v_0 k_0 t	t	t[k_i]=v_i  */

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

CALLFUNC,/*	b c	v_c...v_1 f	r_b...r_1	f(v1,...,v_c)  */

RETCODE,/*	b	-		-  */

SETLINE,/*	b	-		-		LINE=b  */
SETLINEW,/*	w	-		-		LINE=w  */

POP /*		b	-		-		TOP-=(b+1)  */

} OpCode;


#define RFIELDS_PER_FLUSH 32	/* records (SETMAP) */
#define LFIELDS_PER_FLUSH 64    /* lists (SETLIST) */

#define ZEROVARARG	64

#endif
