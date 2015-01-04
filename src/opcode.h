/*
** TeCGraf - PUC-Rio
** $Id: opcode.h,v 3.20 1996/03/15 13:13:13 roberto Exp $
*/

#ifndef opcode_h
#define opcode_h

#include "lua.h"
#include "types.h"
#include "tree.h"
#include "func.h"


#define FIELDS_PER_FLUSH 40


typedef enum 
{ 
 PUSHNIL,
 PUSH0, PUSH1, PUSH2,
 PUSHBYTE,
 PUSHWORD,
 PUSHFLOAT,
 PUSHSTRING,
 PUSHFUNCTION,
 PUSHLOCAL0, PUSHLOCAL1, PUSHLOCAL2, PUSHLOCAL3, PUSHLOCAL4,
 PUSHLOCAL5, PUSHLOCAL6, PUSHLOCAL7, PUSHLOCAL8, PUSHLOCAL9,
 PUSHLOCAL,
 PUSHGLOBAL,
 PUSHINDEXED,
 PUSHSELF,
 STORELOCAL0, STORELOCAL1, STORELOCAL2, STORELOCAL3, STORELOCAL4,
 STORELOCAL5, STORELOCAL6, STORELOCAL7, STORELOCAL8, STORELOCAL9,
 STORELOCAL,
 STOREGLOBAL,
 STOREINDEXED0,
 STOREINDEXED,
 STORELIST0,
 STORELIST,
 STORERECORD,
 ADJUST0,
 ADJUST,
 CREATEARRAY,
 EQOP,
 LTOP,
 LEOP,
 GTOP,
 GEOP,
 ADDOP,
 SUBOP,
 MULTOP,
 DIVOP,
 POWOP,
 CONCOP,
 MINUSOP,
 NOTOP,
 ONTJMP,
 ONFJMP,
 JMP,
 UPJMP,
 IFFJMP,
 IFFUPJMP,
 POP,
 CALLFUNC,
 RETCODE0,
 RETCODE,
 SETLINE
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
#define s_avalue(i)	(avalue(&s_object(i)))
#define s_fvalue(i)	(fvalue(&s_object(i)))
#define s_uvalue(i)	(uvalue(&s_object(i)))

typedef union
{
 struct {Byte c1; Byte c2;} m;
 Word w;
} CodeWord;
#define get_word(code,pc)    {code.m.c1 = *pc++; code.m.c2 = *pc++;}

typedef union
{
 struct {Byte c1; Byte c2; Byte c3; Byte c4;} m;
 float f;
} CodeFloat;
#define get_float(code,pc)   {code.m.c1 = *pc++; code.m.c2 = *pc++;\
                              code.m.c3 = *pc++; code.m.c4 = *pc++;}

typedef union
{
 struct {Byte c1; Byte c2; Byte c3; Byte c4;} m;
 TFunc *tf;
} CodeCode;
#define get_code(code,pc)    {code.m.c1 = *pc++; code.m.c2 = *pc++;\
                              code.m.c3 = *pc++; code.m.c4 = *pc++;}


/* Exported functions */
void    lua_parse      (TFunc *tf);	/* from "lua.stx" module */
void	luaI_codedebugline (int line);  /* from "lua.stx" module */
void    lua_travstack (int (*fn)(Object *));
Object *luaI_Address (lua_Object o);
void	luaI_pushobject (Object *o);
void    luaI_gcFB       (Object *o);
int     luaI_dorun (TFunc *tf);

#endif
