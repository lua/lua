/*
** TeCGraf - PUC-Rio
** $Id: opcode.h,v 3.24 1996/11/01 12:46:59 roberto Exp $
*/

#ifndef opcode_h
#define opcode_h

#include "lua.h"
#include "types.h"
#include "tree.h"
#include "func.h"


#define FIELDS_PER_FLUSH 40


typedef enum {
/* name          parm    before          after           side effect
-----------------------------------------------------------------------------*/

PUSHNIL,/*		-		nil  */
PUSH0,/*		-		0.0  */
PUSH1,/*		-		1.0  */
PUSH2,/*		-		2.0  */
PUSHBYTE,/*	b	-		(float)b  */
PUSHWORD,/*	w	-		(float)w  */
PUSHFLOAT,/*	f	-		f  */
PUSHSTRING,/*	w	-		STR[w]  */
PUSHFUNCTION,/*	p	-		FUN(p)  */
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
PUSHLOCAL,/*	w	-		LOC[w]  */
PUSHGLOBAL,/*	w	-		VAR[w]  */
PUSHINDEXED,/*		i t		t[i]  */
PUSHSELF,/*	w	t		t t[STR[w]]  */
STORELOCAL0,/*		x		-		LOC[0]=x  */
STORELOCAL1,/*		x		-		LOC[1]=x  */
STORELOCAL2,/*		x		-		LOC[2]=x  */
STORELOCAL3,/*		x		-		LOC[3]=x  */
STORELOCAL4,/*		x		-		LOC[4]=x  */
STORELOCAL5,/*		x		-		LOC[5]=x  */
STORELOCAL6,/*		x		-		LOC[6]=x  */
STORELOCAL7,/*		x		-		LOC[7]=x  */
STORELOCAL8,/*		x		-		LOC[8]=x  */
STORELOCAL9,/*		x		-		LOC[9]=x  */
STORELOCAL,/*	w	x		-		LOC[w]=x  */
STOREGLOBAL,/*	w	x		-		VAR[w]=x  */
STOREINDEXED0,/*	v i t		-		t[i]=v  */
STOREINDEXED,/*	b	v a_b...a_1 i t	a_b...a_1 i t	t[i]=v  */
STORELIST0,/*	w	v_w...v_1 t	-		t[i]=v_i  */
STORELIST,/*	w n	v_w...v_1 t	-		t[i+n*FPF]=v_i  */
STORERECORD,/*	n
		w_n...w_1 v_n...v_1 t	-		t[STR[w_i]]=v_i  */
ADJUST0,/*		-		-		TOP=BASE  */
ADJUST,/*	b	-		-		TOP=BASE+b  */
CREATEARRAY,/*	w	-		newarray(size = w)  */
EQOP,/*			y x		(x==y)? 1 : nil  */
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
ONTJMP,/*		w	x	-		(x!=nil)? PC+=w  */
ONFJMP,/*		w	x	-		(x==nil)? PC+=w  */
JMP,/*		w	-		-		PC+=w  */
UPJMP,/*		w	-	-		PC-=w  */
IFFJMP,/*		w	x	-		(x==nil)? PC+=w  */
IFFUPJMP,/*	w	x		-		(x==nil)? PC-=w  */
POP,/*			x		-  */
CALLFUNC,/*	n m	v_n...v_1 f	r_m...r_1	f(v1,...,v_n)  */
RETCODE0,
RETCODE,/*	b	-		-  */
SETLINE,/*	w	-		-		LINE=w  */
VARARGS/*	b	v_n...v_1	{v_1...v_n;n=n}  */

} OpCode;


#define MULT_RET	255


typedef union
{
 lua_CFunction f;
 real          n;
 TaggedString *ts;
 TFunc         *tf;
 struct Hash    *a;
 void           *u;
 int	       i;
} Value;

typedef struct Object
{
 lua_Type  tag;
 Value value;
} Object;


/* Macros to access structure members */
#define tag(o)		((o)->tag)
#define nvalue(o)	((o)->value.n)
#define svalue(o)	((o)->value.ts->str)
#define tsvalue(o)	((o)->value.ts)
#define avalue(o)	((o)->value.a)
#define fvalue(o)	((o)->value.f)
#define uvalue(o)	((o)->value.u)

/* Macros to access symbol table */
#define s_object(i)	(lua_table[i].object)
#define s_tag(i)	(tag(&s_object(i)))
#define s_nvalue(i)	(nvalue(&s_object(i)))
#define s_svalue(i)	(svalue(&s_object(i)))
#define s_tsvalue(i)	(tsvalue(&s_object(i)))
#define s_avalue(i)	(avalue(&s_object(i)))
#define s_fvalue(i)	(fvalue(&s_object(i)))
#define s_uvalue(i)	(uvalue(&s_object(i)))

#define get_word(code,pc) {memcpy(&code, pc, sizeof(Word)); pc+=sizeof(Word);}
#define get_float(code,pc){memcpy(&code, pc, sizeof(real)); pc+=sizeof(real);}
#define get_code(code,pc) {memcpy(&code, pc, sizeof(TFunc *)); \
                           pc+=sizeof(TFunc *);}


/* Exported functions */
void    lua_parse      (TFunc *tf);	/* from "lua.stx" module */
void	luaI_codedebugline (int line);  /* from "lua.stx" module */
void    lua_travstack (int (*fn)(Object *));
Object *luaI_Address (lua_Object o);
void	luaI_pushobject (Object *o);
void    luaI_gcFB       (Object *o);
int     luaI_dorun (TFunc *tf);

#endif
