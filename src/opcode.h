/*
** TeCGraf - PUC-Rio
** $Id: opcode.h,v 2.1 1994/04/20 22:07:57 celes Exp $
*/

#ifndef opcode_h
#define opcode_h

#ifndef STACKGAP
#define STACKGAP	128
#endif 

#ifndef real
#define real float
#endif

#define FIELDS_PER_FLUSH 40

typedef unsigned char  Byte;

typedef unsigned short Word;

typedef union
{
 struct {char c1; char c2;} m;
 Word w;
} CodeWord;

typedef union
{
 struct {char c1; char c2; char c3; char c4;} m;
 float f;
} CodeFloat;

typedef enum 
{ 
 PUSHNIL,
 PUSH0, PUSH1, PUSH2,
 PUSHBYTE,
 PUSHWORD,
 PUSHFLOAT,
 PUSHSTRING,
 PUSHLOCAL0, PUSHLOCAL1, PUSHLOCAL2, PUSHLOCAL3, PUSHLOCAL4,
 PUSHLOCAL5, PUSHLOCAL6, PUSHLOCAL7, PUSHLOCAL8, PUSHLOCAL9,
 PUSHLOCAL,
 PUSHGLOBAL,
 PUSHINDEXED,
 PUSHMARK,
 PUSHOBJECT,
 STORELOCAL0, STORELOCAL1, STORELOCAL2, STORELOCAL3, STORELOCAL4,
 STORELOCAL5, STORELOCAL6, STORELOCAL7, STORELOCAL8, STORELOCAL9,
 STORELOCAL,
 STOREGLOBAL,
 STOREINDEXED0,
 STOREINDEXED,
 STORELIST0,
 STORELIST,
 STORERECORD,
 ADJUST,
 CREATEARRAY,
 EQOP,
 LTOP,
 LEOP,
 ADDOP,
 SUBOP,
 MULTOP,
 DIVOP,
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
 RETCODE,
 HALT,
 SETFUNCTION,
 SETLINE,
 RESET
} OpCode;

typedef enum
{
 T_MARK,
 T_NIL,
 T_NUMBER,
 T_STRING,
 T_ARRAY,
 T_FUNCTION,
 T_CFUNCTION,
 T_USERDATA
} Type; 

typedef void (*Cfunction) (void);
typedef int  (*Input) (void);

typedef union
{
 Cfunction 	 f;
 real    	 n;
 char      	*s;
 Byte      	*b;
 struct Hash    *a;
 void           *u;
} Value;

typedef struct Object
{
 Type  tag;
 Value value;
} Object;

typedef struct
{
 char   *name;
 Object  object;
} Symbol;

/* Macros to access structure members */
#define tag(o)		((o)->tag)
#define nvalue(o)	((o)->value.n)
#define svalue(o)	((o)->value.s)
#define bvalue(o)	((o)->value.b)
#define avalue(o)	((o)->value.a)
#define fvalue(o)	((o)->value.f)
#define uvalue(o)	((o)->value.u)

/* Macros to access symbol table */
#define s_name(i)	(lua_table[i].name)
#define s_object(i)	(lua_table[i].object)
#define s_tag(i)	(tag(&s_object(i)))
#define s_nvalue(i)	(nvalue(&s_object(i)))
#define s_svalue(i)	(svalue(&s_object(i)))
#define s_bvalue(i)	(bvalue(&s_object(i)))
#define s_avalue(i)	(avalue(&s_object(i)))
#define s_fvalue(i)	(fvalue(&s_object(i)))
#define s_uvalue(i)	(uvalue(&s_object(i)))

#define get_word(code,pc)    {code.m.c1 = *pc++; code.m.c2 = *pc++;}
#define get_float(code,pc)   {code.m.c1 = *pc++; code.m.c2 = *pc++;\
                              code.m.c3 = *pc++; code.m.c4 = *pc++;}
 


/* Exported functions */
int     lua_execute   (Byte *pc);
void    lua_markstack (void);
char   *lua_strdup (char *l);

void    lua_setinput   (Input fn);	/* from "lex.c" module */
char   *lua_lasttext   (void);		/* from "lex.c" module */
int     lua_parse      (void); 		/* from "lua.stx" module */
void    lua_type       (void);
void 	lua_obj2number (void);
void 	lua_print      (void);
void 	lua_internaldofile (void);
void 	lua_internaldostring (void);
void    lua_travstack (void (*fn)(Object *));

#endif
