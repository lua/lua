# line 2 "lua.stx"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "opcode.h"
#include "hash.h"
#include "inout.h"
#include "table.h"
#include "lua.h"

#ifndef ALIGNMENT
#define ALIGNMENT	(sizeof(void *))
#endif

#ifndef MAXCODE
#define MAXCODE 1024
#endif
static long   buffer[MAXCODE];
static Byte  *code = (Byte *)buffer;
static long   mainbuffer[MAXCODE];
static Byte  *maincode = (Byte *)mainbuffer;
static Byte  *basepc;
static Byte  *pc;

#define MAXVAR 32
static long    varbuffer[MAXVAR];
static Byte    nvarbuffer=0;	     /* number of variables at a list */

static Word    localvar[STACKGAP];
static Byte    nlocalvar=0;	     /* number of local variables */
static int     ntemp;		     /* number of temporary var into stack */
static int     err;		     /* flag to indicate error */

/* Internal functions */
#define align(n)  align_n(sizeof(n))

static void code_byte (Byte c)
{
 if (pc-basepc>MAXCODE-1)
 {
  lua_error ("code buffer overflow");
  err = 1;
 }
 *pc++ = c;
}

static void code_word (Word n)
{
 if (pc-basepc>MAXCODE-sizeof(Word))
 {
  lua_error ("code buffer overflow");
  err = 1;
 }
 *((Word *)pc) = n;
 pc += sizeof(Word);
}

static void code_float (float n)
{
 if (pc-basepc>MAXCODE-sizeof(float))
 {
  lua_error ("code buffer overflow");
  err = 1;
 }
 *((float *)pc) = n;
 pc += sizeof(float);
}

static void incr_ntemp (void)
{
 if (ntemp+nlocalvar+MAXVAR+1 < STACKGAP)
  ntemp++;
 else
 {
  lua_error ("stack overflow");
  err = 1;
 }
}

static void incr_nlocalvar (void)
{
 if (ntemp+nlocalvar+MAXVAR+1 < STACKGAP)
  nlocalvar++;
 else
 {
  lua_error ("too many local variables or expression too complicate");
  err = 1;
 }
}

static void incr_nvarbuffer (void)
{
 if (nvarbuffer < MAXVAR-1)
  nvarbuffer++;
 else
 {
  lua_error ("variable buffer overflow");
  err = 1;
 }
}

static void align_n (unsigned size)
{
 if (size > ALIGNMENT) size = ALIGNMENT;
 while (((pc+1-code)%size) != 0)	/* +1 to include BYTECODE */
  code_byte (NOP);
}

static void code_number (float f)
{ int i = f;
  if (f == i)  /* f has an integer value */
  {
   if (i <= 2) code_byte(PUSH0 + i);
   else if (i <= 255)
   {
    code_byte(PUSHBYTE);
    code_byte(i);
   }
   else
   {
    align(Word);
    code_byte(PUSHWORD);
    code_word(i);
   }
  }
  else
  {
   align(float);
   code_byte(PUSHFLOAT);
   code_float(f);
  }
  incr_ntemp();
}


# line 140 "lua.stx"
typedef union  
{
 int   vInt;
 long  vLong;
 float vFloat;
 Word  vWord;
 Byte *pByte;
} YYSTYPE;
# define NIL 257
# define IF 258
# define THEN 259
# define ELSE 260
# define ELSEIF 261
# define WHILE 262
# define DO 263
# define REPEAT 264
# define UNTIL 265
# define END 266
# define RETURN 267
# define LOCAL 268
# define NUMBER 269
# define FUNCTION 270
# define NAME 271
# define STRING 272
# define DEBUG 273
# define NOT 274
# define AND 275
# define OR 276
# define NE 277
# define LE 278
# define GE 279
# define CONC 280
# define UNARY 281
#define yyclearin yychar = -1
#define yyerrok yyerrflag = 0
extern int yychar;
extern int yyerrflag;
#ifndef YYMAXDEPTH
#define YYMAXDEPTH 150
#endif
YYSTYPE yylval, yyval;
# define YYERRCODE 256

# line 530 "lua.stx"


/*
** Search a local name and if find return its index. If do not find return -1
*/
static int lua_localname (Word n)
{
 int i;
 for (i=nlocalvar-1; i >= 0; i--)
  if (n == localvar[i]) return i;	/* local var */
 return -1;		        /* global var */
}

/*
** Push a variable given a number. If number is positive, push global variable
** indexed by (number -1). If negative, push local indexed by ABS(number)-1.
** Otherwise, if zero, push indexed variable (record).
*/
static void lua_pushvar (long number)
{ 
 if (number > 0)	/* global var */
 {
  align(Word);
  code_byte(PUSHGLOBAL);
  code_word(number-1);
  incr_ntemp();
 }
 else if (number < 0)	/* local var */
 {
  number = (-number) - 1;
  if (number < 10) code_byte(PUSHLOCAL0 + number);
  else
  {
   code_byte(PUSHLOCAL);
   code_byte(number);
  }
  incr_ntemp();
 }
 else
 {
  code_byte(PUSHINDEXED);
  ntemp--;
 }
}

static void lua_codeadjust (int n)
{
 code_byte(ADJUST);
 code_byte(n + nlocalvar);
}

static void lua_codestore (int i)
{
 if (varbuffer[i] > 0)		/* global var */
 {
  align(Word);
  code_byte(STOREGLOBAL);
  code_word(varbuffer[i]-1);
 }
 else if (varbuffer[i] < 0)      /* local var */
 {
  int number = (-varbuffer[i]) - 1;
  if (number < 10) code_byte(STORELOCAL0 + number);
  else
  {
   code_byte(STORELOCAL);
   code_byte(number);
  }
 }
 else				  /* indexed var */
 {
  int j;
  int upper=0;     	/* number of indexed variables upper */
  int param;		/* number of itens until indexed expression */
  for (j=i+1; j <nvarbuffer; j++)
   if (varbuffer[j] == 0) upper++;
  param = upper*2 + i;
  if (param == 0)
   code_byte(STOREINDEXED0);
  else
  {
   code_byte(STOREINDEXED);
   code_byte(param);
  }
 }
}

void yyerror (char *s)
{
 static char msg[256];
 sprintf (msg,"%s near \"%s\" at line %d in file \"%s\"",
          s, lua_lasttext (), lua_linenumber, lua_filename());
 lua_error (msg);
 err = 1;
}

int yywrap (void)
{
 return 1;
}


/*
** Parse LUA code and execute global statement.
** Return 0 on success or 1 on error.
*/
int lua_parse (void)
{
 Byte *initcode = maincode;
 err = 0;
 if (yyparse () || (err==1)) return 1;
 *maincode++ = HALT;
 if (lua_execute (initcode)) return 1;
 maincode = initcode;
 return 0;
}


#if 0

static void PrintCode (void)
{
 Byte *p = code;
 printf ("\n\nCODE\n");
 while (p != pc)
 {
  switch ((OpCode)*p)
  {
   case NOP:		printf ("%d    NOP\n", (p++)-code); break;
   case PUSHNIL:	printf ("%d    PUSHNIL\n", (p++)-code); break;
   case PUSH0: case PUSH1: case PUSH2:
    			printf ("%d    PUSH%c\n", p-code, *p-PUSH0+'0');
    			p++;
   			break;
   case PUSHBYTE:
    			printf ("%d    PUSHBYTE   %d\n", p-code, *(++p));
    			p++;
   			break;
   case PUSHWORD:
    			printf ("%d    PUSHWORD   %d\n", p-code, *((Word *)(p+1)));
    			p += 1 + sizeof(Word);
   			break;
   case PUSHFLOAT:
    			printf ("%d    PUSHFLOAT  %f\n", p-code, *((float *)(p+1)));
    			p += 1 + sizeof(float);
   			break;
   case PUSHSTRING:
    			printf ("%d    PUSHSTRING   %d\n", p-code, *((Word *)(p+1)));
    			p += 1 + sizeof(Word);
   			break;
   case PUSHLOCAL0: case PUSHLOCAL1: case PUSHLOCAL2: case PUSHLOCAL3:
   case PUSHLOCAL4: case PUSHLOCAL5: case PUSHLOCAL6: case PUSHLOCAL7:
   case PUSHLOCAL8: case PUSHLOCAL9:
    			printf ("%d    PUSHLOCAL%c\n", p-code, *p-PUSHLOCAL0+'0');
    			p++;
   			break;
   case PUSHLOCAL:	printf ("%d    PUSHLOCAL   %d\n", p-code, *(++p));
    			p++;
   			break;
   case PUSHGLOBAL:
    			printf ("%d    PUSHGLOBAL   %d\n", p-code, *((Word *)(p+1)));
    			p += 1 + sizeof(Word);
   			break;
   case PUSHINDEXED:    printf ("%d    PUSHINDEXED\n", (p++)-code); break;
   case PUSHMARK:       printf ("%d    PUSHMARK\n", (p++)-code); break;
   case PUSHOBJECT:     printf ("%d    PUSHOBJECT\n", (p++)-code); break;
   case STORELOCAL0: case STORELOCAL1: case STORELOCAL2: case STORELOCAL3:
   case STORELOCAL4: case STORELOCAL5: case STORELOCAL6: case STORELOCAL7:
   case STORELOCAL8: case STORELOCAL9:
    			printf ("%d    STORELOCAL%c\n", p-code, *p-STORELOCAL0+'0');
    			p++;
   			break;
   case STORELOCAL:
    			printf ("%d    STORELOCAK   %d\n", p-code, *(++p));
    			p++;
   			break;
   case STOREGLOBAL:
    			printf ("%d    STOREGLOBAL   %d\n", p-code, *((Word *)(p+1)));
    			p += 1 + sizeof(Word);
   			break;
   case STOREINDEXED0:  printf ("%d    STOREINDEXED0\n", (p++)-code); break;
   case STOREINDEXED:   printf ("%d    STOREINDEXED   %d\n", p-code, *(++p));
    			p++;
   			break;
   case STOREFIELD:     printf ("%d    STOREFIELD\n", (p++)-code); break;
   case ADJUST:
    			printf ("%d    ADJUST   %d\n", p-code, *(++p));
    			p++;
   			break;
   case CREATEARRAY:	printf ("%d    CREATEARRAY\n", (p++)-code); break;
   case EQOP:       	printf ("%d    EQOP\n", (p++)-code); break;
   case LTOP:       	printf ("%d    LTOP\n", (p++)-code); break;
   case LEOP:       	printf ("%d    LEOP\n", (p++)-code); break;
   case ADDOP:       	printf ("%d    ADDOP\n", (p++)-code); break;
   case SUBOP:       	printf ("%d    SUBOP\n", (p++)-code); break;
   case MULTOP:      	printf ("%d    MULTOP\n", (p++)-code); break;
   case DIVOP:       	printf ("%d    DIVOP\n", (p++)-code); break;
   case CONCOP:       	printf ("%d    CONCOP\n", (p++)-code); break;
   case MINUSOP:       	printf ("%d    MINUSOP\n", (p++)-code); break;
   case NOTOP:       	printf ("%d    NOTOP\n", (p++)-code); break;
   case ONTJMP:	   
    			printf ("%d    ONTJMP  %d\n", p-code, *((Word *)(p+1)));
    			p += sizeof(Word) + 1;
   			break;
   case ONFJMP:	   
    			printf ("%d    ONFJMP  %d\n", p-code, *((Word *)(p+1)));
    			p += sizeof(Word) + 1;
   			break;
   case JMP:	   
    			printf ("%d    JMP  %d\n", p-code, *((Word *)(p+1)));
    			p += sizeof(Word) + 1;
   			break;
   case UPJMP:
    			printf ("%d    UPJMP  %d\n", p-code, *((Word *)(p+1)));
    			p += sizeof(Word) + 1;
   			break;
   case IFFJMP:
    			printf ("%d    IFFJMP  %d\n", p-code, *((Word *)(p+1)));
    			p += sizeof(Word) + 1;
   			break;
   case IFFUPJMP:
    			printf ("%d    IFFUPJMP  %d\n", p-code, *((Word *)(p+1)));
    			p += sizeof(Word) + 1;
   			break;
   case POP:       	printf ("%d    POP\n", (p++)-code); break;
   case CALLFUNC:	printf ("%d    CALLFUNC\n", (p++)-code); break;
   case RETCODE:
    			printf ("%d    RETCODE   %d\n", p-code, *(++p));
    			p++;
   			break;
   default:		printf ("%d    Cannot happen\n", (p++)-code); break;
  }
 }
}
#endif

int yyexca[] ={
-1, 1,
	0, -1,
	-2, 2,
-1, 19,
	40, 65,
	91, 95,
	46, 97,
	-2, 92,
-1, 29,
	40, 65,
	91, 95,
	46, 97,
	-2, 51,
-1, 70,
	275, 33,
	276, 33,
	61, 33,
	277, 33,
	62, 33,
	60, 33,
	278, 33,
	279, 33,
	280, 33,
	43, 33,
	45, 33,
	42, 33,
	47, 33,
	-2, 68,
-1, 71,
	91, 95,
	46, 97,
	-2, 93,
-1, 102,
	260, 27,
	261, 27,
	265, 27,
	266, 27,
	267, 27,
	-2, 11,
-1, 117,
	93, 85,
	-2, 87,
-1, 122,
	267, 30,
	-2, 29,
-1, 145,
	275, 33,
	276, 33,
	61, 33,
	277, 33,
	62, 33,
	60, 33,
	278, 33,
	279, 33,
	280, 33,
	43, 33,
	45, 33,
	42, 33,
	47, 33,
	-2, 70,
	};
# define YYNPROD 105
# define YYLAST 318
int yyact[]={

    54,    52,   136,    53,    13,    55,    54,    52,    14,    53,
    15,    55,     5,   166,    18,     6,   129,    21,    47,    46,
    48,   107,   104,    97,    47,    46,    48,    54,    52,    80,
    53,    21,    55,    54,    52,    40,    53,     9,    55,    54,
    52,   158,    53,   160,    55,    47,    46,    48,   159,   101,
    81,    47,    46,    48,    10,    54,    52,   126,    53,    67,
    55,    54,    52,    60,    53,   155,    55,   148,   149,   135,
   147,   108,   150,    47,    46,    48,    73,    23,    75,    47,
    46,    48,     7,    25,    38,   153,    26,   164,    27,   117,
    61,    62,    74,    11,    76,    54,    24,   127,    65,    66,
    55,    37,   154,   151,   103,   111,    72,    28,    93,    94,
    82,    83,    84,    85,    86,    87,    88,    89,    90,    91,
    92,   116,    59,    77,    54,    52,   118,    53,    99,    55,
   110,    95,    64,    44,    70,   109,    29,    33,   105,   106,
    42,   112,    41,   165,   139,    19,    17,   152,    79,   123,
    43,   119,    20,   114,   113,    98,    63,   144,   143,   122,
    68,    39,    36,   130,    35,   120,    12,     8,   102,   125,
   128,   141,    78,    69,    70,    71,   142,   131,   132,   140,
    22,   124,     4,     3,     2,   121,    96,   138,   146,   137,
   134,   157,   133,   115,    16,     1,     0,     0,     0,     0,
     0,     0,     0,   156,     0,     0,     0,     0,   161,     0,
     0,     0,     0,   162,     0,     0,     0,   168,     0,   172,
   145,   163,   171,     0,   174,     0,     0,     0,   169,   156,
   167,   170,   173,    57,    58,    49,    50,    51,    56,    57,
    58,    49,    50,    51,    56,   175,     0,     0,   100,     0,
    45,     0,     0,     0,     0,    70,     0,     0,     0,     0,
    57,    58,    49,    50,    51,    56,    57,    58,    49,    50,
    51,    56,     0,     0,     0,     0,     0,    56,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,    57,    58,
    49,    50,    51,    56,     0,     0,    49,    50,    51,    56,
    32,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,    30,     0,    21,    31,     0,    34 };
int yypact[]={

 -1000,  -258, -1000, -1000, -1000,  -234, -1000,    34,  -254, -1000,
 -1000, -1000, -1000,    43, -1000, -1000,    40, -1000,  -236, -1000,
 -1000, -1000,    93,    -9, -1000,    43,    43,    43,    92, -1000,
 -1000, -1000, -1000, -1000,    43,    43, -1000,    43,  -240,    62,
    31,   -13,    48,    83,  -242, -1000,    43,    43,    43,    43,
    43,    43,    43,    43,    43,    43,    43, -1000, -1000,    90,
    13, -1000, -1000,  -248,    43,    19,   -15,  -216, -1000,    60,
 -1000, -1000,  -249, -1000, -1000,    43,  -250,    43,    89,    61,
 -1000, -1000,    -3,    -3,    -3,    -3,    -3,    -3,    53,    53,
 -1000, -1000,    82, -1000, -1000, -1000,    -2, -1000,    85,    13,
 -1000,    43, -1000, -1000,    31,    43,   -36, -1000,    56,    60,
 -1000,  -255, -1000,    43,    43, -1000,  -269, -1000, -1000, -1000,
    13,    34, -1000,    43, -1000,    13, -1000, -1000, -1000, -1000,
  -193,    19,    19,   -53,    59, -1000, -1000,    -8,    58,    43,
 -1000, -1000, -1000, -1000,  -226, -1000,  -218,  -223, -1000,    43,
 -1000,  -269,    26, -1000, -1000, -1000,    13,  -253,    43, -1000,
 -1000, -1000,   -42, -1000,    43,    43, -1000,    34, -1000,    13,
 -1000, -1000, -1000, -1000,  -193, -1000 };
int yypgo[]={

     0,   195,    50,    96,    71,   135,   194,   193,   192,   190,
   189,   187,   136,   186,   184,    82,    54,   183,   182,   180,
   172,   170,    59,   168,   167,   166,    63,    70,   164,   162,
   137,   161,   160,   159,   158,   157,   156,   155,   154,   153,
   152,   150,   149,   148,    69,   147,   144,    65,   143,   142,
   140,    76,   138 };
int yyr1[]={

     0,     1,    14,     1,     1,     1,    19,    21,    17,    23,
    23,    24,    15,    16,    16,    25,    28,    25,    29,    25,
    25,    25,    25,    27,    27,    27,    32,    33,    22,    34,
    35,    34,     2,    26,     3,     3,     3,     3,     3,     3,
     3,     3,     3,     3,     3,     3,     3,     3,    36,     3,
     3,     3,     3,     3,     3,     3,     3,    38,     3,    39,
     3,    37,    37,    41,    30,    40,     4,     4,     5,    42,
     5,    20,    20,    43,    43,    13,    13,     7,     7,     8,
     8,     9,     9,    45,    44,    10,    10,    46,    11,    48,
    11,    47,     6,     6,    12,    49,    12,    50,    12,    31,
    31,    51,    52,    51,    18 };
int yyr2[]={

     0,     0,     1,     9,     4,     4,     1,     1,    19,     0,
     6,     1,     4,     0,     2,    17,     1,    17,     1,    13,
     7,     3,     4,     0,     4,    15,     1,     1,     9,     0,
     1,     9,     1,     3,     7,     7,     7,     7,     7,     7,
     7,     7,     7,     7,     7,     7,     5,     5,     1,     9,
     9,     3,     3,     3,     3,     3,     5,     1,    11,     1,
    11,     1,     2,     1,    11,     3,     1,     3,     3,     1,
     9,     0,     2,     3,     7,     1,     3,     7,     7,     1,
     3,     3,     7,     1,     9,     1,     3,     1,     5,     1,
     9,     3,     3,     7,     3,     1,    11,     1,     9,     5,
     9,     1,     1,     6,     3 };
int yychk[]={

 -1000,    -1,   -14,   -17,   -18,   270,   273,   -15,   -24,   271,
   -16,    59,   -25,   258,   262,   264,    -6,   -30,   268,   -12,
   -40,   271,   -19,   -26,    -3,    40,    43,    45,    64,   -12,
   269,   272,   257,   -30,   274,   -28,   -29,    61,    44,   -31,
   271,   -49,   -50,   -41,    40,   259,    61,    60,    62,   277,
   278,   279,    43,    45,    42,    47,   280,   275,   276,    -3,
   -26,   -26,   -26,   -36,    40,   -26,   -26,   -22,   -32,    -5,
    -3,   -12,    44,   -51,    61,    91,    46,    40,   -20,   -43,
   271,    -2,   -26,   -26,   -26,   -26,   -26,   -26,   -26,   -26,
   -26,   -26,   -26,    -2,    -2,    41,   -13,   271,   -37,   -26,
   263,   265,   -23,    44,   271,   -52,   -26,   271,    -4,    -5,
    41,    44,   -22,   -38,   -39,    -7,   123,    91,    41,    -2,
   -26,   -15,   -33,   -42,   -51,   -26,    93,    41,   -21,   271,
    -2,   -26,   -26,    -8,    -9,   -44,   271,   -10,   -11,   -46,
   -22,    -2,   -16,   -34,   -35,    -3,   -22,   -27,   260,   261,
   125,    44,   -45,    93,    44,   -47,   -26,    -2,   267,   266,
   266,   -22,   -26,   -44,    61,   -48,   266,    -4,   259,   -26,
   -47,   -16,    -2,   -22,    -2,   -27 };
int yydef[]={

     1,    -2,    11,     4,     5,     0,   104,    13,     0,     6,
     3,    14,    12,     0,    16,    18,     0,    21,     0,    -2,
    63,    94,     0,     0,    33,     0,     0,     0,    48,    -2,
    52,    53,    54,    55,     0,     0,    26,     0,     0,    22,
   101,     0,     0,     0,    71,    32,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,    32,    32,    33,
     0,    46,    47,    75,    61,    56,     0,     0,     9,    20,
    -2,    -2,     0,    99,   102,     0,     0,    66,     0,    72,
    73,    26,    35,    36,    37,    38,    39,    40,    41,    42,
    43,    44,    45,    57,    59,    34,     0,    76,     0,    62,
    32,     0,    -2,    69,   101,     0,     0,    98,     0,    67,
     7,     0,    32,     0,     0,    49,    79,    -2,    50,    26,
    32,    13,    -2,     0,   100,   103,    96,    64,    26,    74,
    23,    58,    60,     0,    80,    81,    83,     0,    86,     0,
    32,    19,    10,    28,     0,    -2,     0,     0,    26,     0,
    77,     0,     0,    78,    89,    88,    91,     0,    66,     8,
    15,    24,     0,    82,     0,     0,    17,    13,    32,    84,
    90,    31,    26,    32,    23,    25 };
typedef struct { char *t_name; int t_val; } yytoktype;
#ifndef YYDEBUG
#	define YYDEBUG	0	/* don't allow debugging */
#endif

#if YYDEBUG

yytoktype yytoks[] =
{
	"NIL",	257,
	"IF",	258,
	"THEN",	259,
	"ELSE",	260,
	"ELSEIF",	261,
	"WHILE",	262,
	"DO",	263,
	"REPEAT",	264,
	"UNTIL",	265,
	"END",	266,
	"RETURN",	267,
	"LOCAL",	268,
	"NUMBER",	269,
	"FUNCTION",	270,
	"NAME",	271,
	"STRING",	272,
	"DEBUG",	273,
	"NOT",	274,
	"AND",	275,
	"OR",	276,
	"=",	61,
	"NE",	277,
	">",	62,
	"<",	60,
	"LE",	278,
	"GE",	279,
	"CONC",	280,
	"+",	43,
	"-",	45,
	"*",	42,
	"/",	47,
	"%",	37,
	"UNARY",	281,
	"-unknown-",	-1	/* ends search */
};

char * yyreds[] =
{
	"-no such reduction-",
	"functionlist : /* empty */",
	"functionlist : functionlist",
	"functionlist : functionlist stat sc",
	"functionlist : functionlist function",
	"functionlist : functionlist setdebug",
	"function : FUNCTION NAME",
	"function : FUNCTION NAME '(' parlist ')'",
	"function : FUNCTION NAME '(' parlist ')' block END",
	"statlist : /* empty */",
	"statlist : statlist stat sc",
	"stat : /* empty */",
	"stat : stat1",
	"sc : /* empty */",
	"sc : ';'",
	"stat1 : IF expr1 THEN PrepJump block PrepJump elsepart END",
	"stat1 : WHILE",
	"stat1 : WHILE expr1 DO PrepJump block PrepJump END",
	"stat1 : REPEAT",
	"stat1 : REPEAT block UNTIL expr1 PrepJump",
	"stat1 : varlist1 '=' exprlist1",
	"stat1 : functioncall",
	"stat1 : LOCAL declist",
	"elsepart : /* empty */",
	"elsepart : ELSE block",
	"elsepart : ELSEIF expr1 THEN PrepJump block PrepJump elsepart",
	"block : /* empty */",
	"block : statlist",
	"block : statlist ret",
	"ret : /* empty */",
	"ret : /* empty */",
	"ret : RETURN exprlist sc",
	"PrepJump : /* empty */",
	"expr1 : expr",
	"expr : '(' expr ')'",
	"expr : expr1 '=' expr1",
	"expr : expr1 '<' expr1",
	"expr : expr1 '>' expr1",
	"expr : expr1 NE expr1",
	"expr : expr1 LE expr1",
	"expr : expr1 GE expr1",
	"expr : expr1 '+' expr1",
	"expr : expr1 '-' expr1",
	"expr : expr1 '*' expr1",
	"expr : expr1 '/' expr1",
	"expr : expr1 CONC expr1",
	"expr : '+' expr1",
	"expr : '-' expr1",
	"expr : '@'",
	"expr : '@' objectname fieldlist",
	"expr : '@' '(' dimension ')'",
	"expr : var",
	"expr : NUMBER",
	"expr : STRING",
	"expr : NIL",
	"expr : functioncall",
	"expr : NOT expr1",
	"expr : expr1 AND PrepJump",
	"expr : expr1 AND PrepJump expr1",
	"expr : expr1 OR PrepJump",
	"expr : expr1 OR PrepJump expr1",
	"dimension : /* empty */",
	"dimension : expr1",
	"functioncall : functionvalue",
	"functioncall : functionvalue '(' exprlist ')'",
	"functionvalue : var",
	"exprlist : /* empty */",
	"exprlist : exprlist1",
	"exprlist1 : expr",
	"exprlist1 : exprlist1 ','",
	"exprlist1 : exprlist1 ',' expr",
	"parlist : /* empty */",
	"parlist : parlist1",
	"parlist1 : NAME",
	"parlist1 : parlist1 ',' NAME",
	"objectname : /* empty */",
	"objectname : NAME",
	"fieldlist : '{' ffieldlist '}'",
	"fieldlist : '[' lfieldlist ']'",
	"ffieldlist : /* empty */",
	"ffieldlist : ffieldlist1",
	"ffieldlist1 : ffield",
	"ffieldlist1 : ffieldlist1 ',' ffield",
	"ffield : NAME",
	"ffield : NAME '=' expr1",
	"lfieldlist : /* empty */",
	"lfieldlist : lfieldlist1",
	"lfieldlist1 : /* empty */",
	"lfieldlist1 : lfield",
	"lfieldlist1 : lfieldlist1 ','",
	"lfieldlist1 : lfieldlist1 ',' lfield",
	"lfield : expr1",
	"varlist1 : var",
	"varlist1 : varlist1 ',' var",
	"var : NAME",
	"var : var",
	"var : var '[' expr1 ']'",
	"var : var",
	"var : var '.' NAME",
	"declist : NAME init",
	"declist : declist ',' NAME init",
	"init : /* empty */",
	"init : '='",
	"init : '=' expr1",
	"setdebug : DEBUG",
};
#endif /* YYDEBUG */
#line 1 "/usr/lib/yaccpar"
/*	@(#)yaccpar 1.10 89/04/04 SMI; from S5R3 1.10	*/

/*
** Skeleton parser driver for yacc output
*/

/*
** yacc user known macros and defines
*/
#define YYERROR		goto yyerrlab
#define YYACCEPT	{ free(yys); free(yyv); return(0); }
#define YYABORT		{ free(yys); free(yyv); return(1); }
#define YYBACKUP( newtoken, newvalue )\
{\
	if ( yychar >= 0 || ( yyr2[ yytmp ] >> 1 ) != 1 )\
	{\
		yyerror( "syntax error - cannot backup" );\
		goto yyerrlab;\
	}\
	yychar = newtoken;\
	yystate = *yyps;\
	yylval = newvalue;\
	goto yynewstate;\
}
#define YYRECOVERING()	(!!yyerrflag)
#ifndef YYDEBUG
#	define YYDEBUG	1	/* make debugging available */
#endif

/*
** user known globals
*/
int yydebug;			/* set to 1 to get debugging */

/*
** driver internal defines
*/
#define YYFLAG		(-1000)

/*
** static variables used by the parser
*/
static YYSTYPE *yyv;			/* value stack */
static int *yys;			/* state stack */

static YYSTYPE *yypv;			/* top of value stack */
static int *yyps;			/* top of state stack */

static int yystate;			/* current state */
static int yytmp;			/* extra var (lasts between blocks) */

int yynerrs;			/* number of errors */

int yyerrflag;			/* error recovery flag */
int yychar;			/* current input token number */


/*
** yyparse - return 0 if worked, 1 if syntax error not recovered from
*/
int
yyparse()
{
	register YYSTYPE *yypvt;	/* top of value stack for $vars */
	unsigned yymaxdepth = YYMAXDEPTH;

	/*
	** Initialize externals - yyparse may be called more than once
	*/
	yyv = (YYSTYPE*)malloc(yymaxdepth*sizeof(YYSTYPE));
	yys = (int*)malloc(yymaxdepth*sizeof(int));
	if (!yyv || !yys)
	{
		yyerror( "out of memory" );
		return(1);
	}
	yypv = &yyv[-1];
	yyps = &yys[-1];
	yystate = 0;
	yytmp = 0;
	yynerrs = 0;
	yyerrflag = 0;
	yychar = -1;

	goto yystack;
	{
		register YYSTYPE *yy_pv;	/* top of value stack */
		register int *yy_ps;		/* top of state stack */
		register int yy_state;		/* current state */
		register int  yy_n;		/* internal state number info */

		/*
		** get globals into registers.
		** branch to here only if YYBACKUP was called.
		*/
	yynewstate:
		yy_pv = yypv;
		yy_ps = yyps;
		yy_state = yystate;
		goto yy_newstate;

		/*
		** get globals into registers.
		** either we just started, or we just finished a reduction
		*/
	yystack:
		yy_pv = yypv;
		yy_ps = yyps;
		yy_state = yystate;

		/*
		** top of for (;;) loop while no reductions done
		*/
	yy_stack:
		/*
		** put a state and value onto the stacks
		*/
#if YYDEBUG
		/*
		** if debugging, look up token value in list of value vs.
		** name pairs.  0 and negative (-1) are special values.
		** Note: linear search is used since time is not a real
		** consideration while debugging.
		*/
		if ( yydebug )
		{
			register int yy_i;

			(void)printf( "State %d, token ", yy_state );
			if ( yychar == 0 )
				(void)printf( "end-of-file\n" );
			else if ( yychar < 0 )
				(void)printf( "-none-\n" );
			else
			{
				for ( yy_i = 0; yytoks[yy_i].t_val >= 0;
					yy_i++ )
				{
					if ( yytoks[yy_i].t_val == yychar )
						break;
				}
				(void)printf( "%s\n", yytoks[yy_i].t_name );
			}
		}
#endif /* YYDEBUG */
		if ( ++yy_ps >= &yys[ yymaxdepth ] )	/* room on stack? */
		{
			/*
			** reallocate and recover.  Note that pointers
			** have to be reset, or bad things will happen
			*/
			int yyps_index = (yy_ps - yys);
			int yypv_index = (yy_pv - yyv);
			int yypvt_index = (yypvt - yyv);
			yymaxdepth += YYMAXDEPTH;
			yyv = (YYSTYPE*)realloc((char*)yyv,
				yymaxdepth * sizeof(YYSTYPE));
			yys = (int*)realloc((char*)yys,
				yymaxdepth * sizeof(int));
			if (!yyv || !yys)
			{
				yyerror( "yacc stack overflow" );
				return(1);
			}
			yy_ps = yys + yyps_index;
			yy_pv = yyv + yypv_index;
			yypvt = yyv + yypvt_index;
		}
		*yy_ps = yy_state;
		*++yy_pv = yyval;

		/*
		** we have a new state - find out what to do
		*/
	yy_newstate:
		if ( ( yy_n = yypact[ yy_state ] ) <= YYFLAG )
			goto yydefault;		/* simple state */
#if YYDEBUG
		/*
		** if debugging, need to mark whether new token grabbed
		*/
		yytmp = yychar < 0;
#endif
		if ( ( yychar < 0 ) && ( ( yychar = yylex() ) < 0 ) )
			yychar = 0;		/* reached EOF */
#if YYDEBUG
		if ( yydebug && yytmp )
		{
			register int yy_i;

			(void)printf( "Received token " );
			if ( yychar == 0 )
				(void)printf( "end-of-file\n" );
			else if ( yychar < 0 )
				(void)printf( "-none-\n" );
			else
			{
				for ( yy_i = 0; yytoks[yy_i].t_val >= 0;
					yy_i++ )
				{
					if ( yytoks[yy_i].t_val == yychar )
						break;
				}
				(void)printf( "%s\n", yytoks[yy_i].t_name );
			}
		}
#endif /* YYDEBUG */
		if ( ( ( yy_n += yychar ) < 0 ) || ( yy_n >= YYLAST ) )
			goto yydefault;
		if ( yychk[ yy_n = yyact[ yy_n ] ] == yychar )	/*valid shift*/
		{
			yychar = -1;
			yyval = yylval;
			yy_state = yy_n;
			if ( yyerrflag > 0 )
				yyerrflag--;
			goto yy_stack;
		}

	yydefault:
		if ( ( yy_n = yydef[ yy_state ] ) == -2 )
		{
#if YYDEBUG
			yytmp = yychar < 0;
#endif
			if ( ( yychar < 0 ) && ( ( yychar = yylex() ) < 0 ) )
				yychar = 0;		/* reached EOF */
#if YYDEBUG
			if ( yydebug && yytmp )
			{
				register int yy_i;

				(void)printf( "Received token " );
				if ( yychar == 0 )
					(void)printf( "end-of-file\n" );
				else if ( yychar < 0 )
					(void)printf( "-none-\n" );
				else
				{
					for ( yy_i = 0;
						yytoks[yy_i].t_val >= 0;
						yy_i++ )
					{
						if ( yytoks[yy_i].t_val
							== yychar )
						{
							break;
						}
					}
					(void)printf( "%s\n", yytoks[yy_i].t_name );
				}
			}
#endif /* YYDEBUG */
			/*
			** look through exception table
			*/
			{
				register int *yyxi = yyexca;

				while ( ( *yyxi != -1 ) ||
					( yyxi[1] != yy_state ) )
				{
					yyxi += 2;
				}
				while ( ( *(yyxi += 2) >= 0 ) &&
					( *yyxi != yychar ) )
					;
				if ( ( yy_n = yyxi[1] ) < 0 )
					YYACCEPT;
			}
		}

		/*
		** check for syntax error
		*/
		if ( yy_n == 0 )	/* have an error */
		{
			/* no worry about speed here! */
			switch ( yyerrflag )
			{
			case 0:		/* new error */
				yyerror( "syntax error" );
				goto skip_init;
			yyerrlab:
				/*
				** get globals into registers.
				** we have a user generated syntax type error
				*/
				yy_pv = yypv;
				yy_ps = yyps;
				yy_state = yystate;
				yynerrs++;
			skip_init:
			case 1:
			case 2:		/* incompletely recovered error */
					/* try again... */
				yyerrflag = 3;
				/*
				** find state where "error" is a legal
				** shift action
				*/
				while ( yy_ps >= yys )
				{
					yy_n = yypact[ *yy_ps ] + YYERRCODE;
					if ( yy_n >= 0 && yy_n < YYLAST &&
						yychk[yyact[yy_n]] == YYERRCODE)					{
						/*
						** simulate shift of "error"
						*/
						yy_state = yyact[ yy_n ];
						goto yy_stack;
					}
					/*
					** current state has no shift on
					** "error", pop stack
					*/
#if YYDEBUG
#	define _POP_ "Error recovery pops state %d, uncovers state %d\n"
					if ( yydebug )
						(void)printf( _POP_, *yy_ps,
							yy_ps[-1] );
#	undef _POP_
#endif
					yy_ps--;
					yy_pv--;
				}
				/*
				** there is no state on stack with "error" as
				** a valid shift.  give up.
				*/
				YYABORT;
			case 3:		/* no shift yet; eat a token */
#if YYDEBUG
				/*
				** if debugging, look up token in list of
				** pairs.  0 and negative shouldn't occur,
				** but since timing doesn't matter when
				** debugging, it doesn't hurt to leave the
				** tests here.
				*/
				if ( yydebug )
				{
					register int yy_i;

					(void)printf( "Error recovery discards " );
					if ( yychar == 0 )
						(void)printf( "token end-of-file\n" );
					else if ( yychar < 0 )
						(void)printf( "token -none-\n" );
					else
					{
						for ( yy_i = 0;
							yytoks[yy_i].t_val >= 0;
							yy_i++ )
						{
							if ( yytoks[yy_i].t_val
								== yychar )
							{
								break;
							}
						}
						(void)printf( "token %s\n",
							yytoks[yy_i].t_name );
					}
				}
#endif /* YYDEBUG */
				if ( yychar == 0 )	/* reached EOF. quit */
					YYABORT;
				yychar = -1;
				goto yy_newstate;
			}
		}/* end if ( yy_n == 0 ) */
		/*
		** reduction by production yy_n
		** put stack tops, etc. so things right after switch
		*/
#if YYDEBUG
		/*
		** if debugging, print the string that is the user's
		** specification of the reduction which is just about
		** to be done.
		*/
		if ( yydebug )
			(void)printf( "Reduce by (%d) \"%s\"\n",
				yy_n, yyreds[ yy_n ] );
#endif
		yytmp = yy_n;			/* value to switch over */
		yypvt = yy_pv;			/* $vars top of value stack */
		/*
		** Look in goto table for next state
		** Sorry about using yy_state here as temporary
		** register variable, but why not, if it works...
		** If yyr2[ yy_n ] doesn't have the low order bit
		** set, then there is no action to be done for
		** this reduction.  So, no saving & unsaving of
		** registers done.  The only difference between the
		** code just after the if and the body of the if is
		** the goto yy_stack in the body.  This way the test
		** can be made before the choice of what to do is needed.
		*/
		{
			/* length of production doubled with extra bit */
			register int yy_len = yyr2[ yy_n ];

			if ( !( yy_len & 01 ) )
			{
				yy_len >>= 1;
				yyval = ( yy_pv -= yy_len )[1];	/* $$ = $1 */
				yy_state = yypgo[ yy_n = yyr1[ yy_n ] ] +
					*( yy_ps -= yy_len ) + 1;
				if ( yy_state >= YYLAST ||
					yychk[ yy_state =
					yyact[ yy_state ] ] != -yy_n )
				{
					yy_state = yyact[ yypgo[ yy_n ] ];
				}
				goto yy_stack;
			}
			yy_len >>= 1;
			yyval = ( yy_pv -= yy_len )[1];	/* $$ = $1 */
			yy_state = yypgo[ yy_n = yyr1[ yy_n ] ] +
				*( yy_ps -= yy_len ) + 1;
			if ( yy_state >= YYLAST ||
				yychk[ yy_state = yyact[ yy_state ] ] != -yy_n )
			{
				yy_state = yyact[ yypgo[ yy_n ] ];
			}
		}
					/* save until reenter driver code */
		yystate = yy_state;
		yyps = yy_ps;
		yypv = yy_pv;
	}
	/*
	** code supplied by user is placed in this switch
	*/
	switch( yytmp )
	{
		
case 2:
# line 179 "lua.stx"
{pc=basepc=maincode; nlocalvar=0;} break;
case 3:
# line 179 "lua.stx"
{maincode=pc;} break;
case 6:
# line 184 "lua.stx"
{pc=basepc=code; nlocalvar=0;} break;
case 7:
# line 185 "lua.stx"
{
	        if (lua_debug)
		{
		 align(Word);
	         code_byte(SETFUNCTION); 
                 code_word(yypvt[-5].vWord);
		 code_word(yypvt[-4].vWord);
		}
	        lua_codeadjust (0);
	       } break;
case 8:
# line 197 "lua.stx"
{ 
                if (lua_debug) code_byte(RESET); 
	        code_byte(RETCODE); code_byte(nlocalvar);
	        s_tag(yypvt[-7].vWord) = T_FUNCTION;
	        s_bvalue(yypvt[-7].vWord) = calloc (pc-code, sizeof(Byte));
	        memcpy (s_bvalue(yypvt[-7].vWord), code, (pc-code)*sizeof(Byte));
	       } break;
case 11:
# line 210 "lua.stx"
{
            ntemp = 0; 
            if (lua_debug)
            {
             align(Word); code_byte(SETLINE); code_word(lua_linenumber);
            }
	   } break;
case 15:
# line 223 "lua.stx"
{
        {
	 Byte *elseinit = yypvt[-2].pByte + sizeof(Word)+1;
	 if (pc - elseinit == 0)		/* no else */
	 {
	  pc -= sizeof(Word)+1;
	 /* if (*(pc-1) == NOP) --pc; */
	  elseinit = pc;
	 }
	 else
	 {
	  *(yypvt[-2].pByte) = JMP;
	  *((Word *)(yypvt[-2].pByte+1)) = pc - elseinit;
	 }
	 *(yypvt[-4].pByte) = IFFJMP;
	 *((Word *)(yypvt[-4].pByte+1)) = elseinit - (yypvt[-4].pByte + sizeof(Word)+1);
	}
       } break;
case 16:
# line 242 "lua.stx"
{yyval.pByte = pc;} break;
case 17:
# line 244 "lua.stx"
{
        *(yypvt[-3].pByte) = IFFJMP;
        *((Word *)(yypvt[-3].pByte+1)) = pc - (yypvt[-3].pByte + sizeof(Word)+1);
        
        *(yypvt[-1].pByte) = UPJMP;
        *((Word *)(yypvt[-1].pByte+1)) = pc - yypvt[-6].pByte;
       } break;
case 18:
# line 252 "lua.stx"
{yyval.pByte = pc;} break;
case 19:
# line 254 "lua.stx"
{
        *(yypvt[-0].pByte) = IFFUPJMP;
        *((Word *)(yypvt[-0].pByte+1)) = pc - yypvt[-4].pByte;
       } break;
case 20:
# line 261 "lua.stx"
{
        {
         int i;
         if (yypvt[-0].vInt == 0 || nvarbuffer != ntemp - yypvt[-2].vInt * 2)
	  lua_codeadjust (yypvt[-2].vInt * 2 + nvarbuffer);
	 for (i=nvarbuffer-1; i>=0; i--)
	  lua_codestore (i);
	 if (yypvt[-2].vInt > 1 || (yypvt[-2].vInt == 1 && varbuffer[0] != 0))
	  lua_codeadjust (0);
	}
       } break;
case 21:
# line 272 "lua.stx"
{ lua_codeadjust (0); } break;
case 25:
# line 279 "lua.stx"
{
          {
  	   Byte *elseinit = yypvt[-1].pByte + sizeof(Word)+1;
  	   if (pc - elseinit == 0)		/* no else */
  	   {
  	    pc -= sizeof(Word)+1;
  	    /* if (*(pc-1) == NOP) --pc; */
	    elseinit = pc;
	   }
	   else
	   {
	    *(yypvt[-1].pByte) = JMP;
	    *((Word *)(yypvt[-1].pByte+1)) = pc - elseinit;
	   }
	   *(yypvt[-3].pByte) = IFFJMP;
	   *((Word *)(yypvt[-3].pByte+1)) = elseinit - (yypvt[-3].pByte + sizeof(Word)+1);
	  }  
         } break;
case 26:
# line 299 "lua.stx"
{yyval.vInt = nlocalvar;} break;
case 27:
# line 299 "lua.stx"
{ntemp = 0;} break;
case 28:
# line 300 "lua.stx"
{
	  if (nlocalvar != yypvt[-3].vInt)
	  {
           nlocalvar = yypvt[-3].vInt;
	   lua_codeadjust (0);
	  }
         } break;
case 30:
# line 310 "lua.stx"
{ if (lua_debug){align(Word);code_byte(SETLINE);code_word(lua_linenumber);}} break;
case 31:
# line 312 "lua.stx"
{ 
           if (lua_debug) code_byte(RESET); 
           code_byte(RETCODE); code_byte(nlocalvar);
          } break;
case 32:
# line 319 "lua.stx"
{ 
          align(Word); 
	  yyval.pByte = pc;
	  code_byte(0);		/* open space */
	  code_word (0);
         } break;
case 33:
# line 326 "lua.stx"
{ if (yypvt[-0].vInt == 0) {lua_codeadjust (ntemp+1); incr_ntemp();}} break;
case 34:
# line 329 "lua.stx"
{ yyval.vInt = yypvt[-1].vInt; } break;
case 35:
# line 330 "lua.stx"
{ code_byte(EQOP);   yyval.vInt = 1; ntemp--;} break;
case 36:
# line 331 "lua.stx"
{ code_byte(LTOP);   yyval.vInt = 1; ntemp--;} break;
case 37:
# line 332 "lua.stx"
{ code_byte(LEOP); code_byte(NOTOP); yyval.vInt = 1; ntemp--;} break;
case 38:
# line 333 "lua.stx"
{ code_byte(EQOP); code_byte(NOTOP); yyval.vInt = 1; ntemp--;} break;
case 39:
# line 334 "lua.stx"
{ code_byte(LEOP);   yyval.vInt = 1; ntemp--;} break;
case 40:
# line 335 "lua.stx"
{ code_byte(LTOP); code_byte(NOTOP); yyval.vInt = 1; ntemp--;} break;
case 41:
# line 336 "lua.stx"
{ code_byte(ADDOP);  yyval.vInt = 1; ntemp--;} break;
case 42:
# line 337 "lua.stx"
{ code_byte(SUBOP);  yyval.vInt = 1; ntemp--;} break;
case 43:
# line 338 "lua.stx"
{ code_byte(MULTOP); yyval.vInt = 1; ntemp--;} break;
case 44:
# line 339 "lua.stx"
{ code_byte(DIVOP);  yyval.vInt = 1; ntemp--;} break;
case 45:
# line 340 "lua.stx"
{ code_byte(CONCOP);  yyval.vInt = 1; ntemp--;} break;
case 46:
# line 341 "lua.stx"
{ yyval.vInt = 1; } break;
case 47:
# line 342 "lua.stx"
{ code_byte(MINUSOP); yyval.vInt = 1;} break;
case 48:
# line 344 "lua.stx"
{
      code_byte(PUSHBYTE);
      yyval.pByte = pc; code_byte(0);
      incr_ntemp();
      code_byte(CREATEARRAY);
     } break;
case 49:
# line 351 "lua.stx"
{
      *(yypvt[-2].pByte) = yypvt[-0].vInt; 
      if (yypvt[-1].vLong < 0)	/* there is no function to be called */
      {
       yyval.vInt = 1;
      }
      else
      {
       lua_pushvar (yypvt[-1].vLong+1);
       code_byte(PUSHMARK);
       incr_ntemp();
       code_byte(PUSHOBJECT);
       incr_ntemp();
       code_byte(CALLFUNC); 
       ntemp -= 4;
       yyval.vInt = 0;
       if (lua_debug)
       {
        align(Word); code_byte(SETLINE); code_word(lua_linenumber);
       }
      }
     } break;
case 50:
# line 374 "lua.stx"
{ 
      code_byte(CREATEARRAY);
      yyval.vInt = 1;
     } break;
case 51:
# line 378 "lua.stx"
{ lua_pushvar (yypvt[-0].vLong); yyval.vInt = 1;} break;
case 52:
# line 379 "lua.stx"
{ code_number(yypvt[-0].vFloat); yyval.vInt = 1; } break;
case 53:
# line 381 "lua.stx"
{
      align(Word);
      code_byte(PUSHSTRING);
      code_word(yypvt[-0].vWord);
      yyval.vInt = 1;
      incr_ntemp();
     } break;
case 54:
# line 388 "lua.stx"
{code_byte(PUSHNIL); yyval.vInt = 1; incr_ntemp();} break;
case 55:
# line 390 "lua.stx"
{
      yyval.vInt = 0;
      if (lua_debug)
      {
       align(Word); code_byte(SETLINE); code_word(lua_linenumber);
      }
     } break;
case 56:
# line 397 "lua.stx"
{ code_byte(NOTOP);  yyval.vInt = 1;} break;
case 57:
# line 398 "lua.stx"
{code_byte(POP); ntemp--;} break;
case 58:
# line 399 "lua.stx"
{ 
      *(yypvt[-2].pByte) = ONFJMP;
      *((Word *)(yypvt[-2].pByte+1)) = pc - (yypvt[-2].pByte + sizeof(Word)+1);
      yyval.vInt = 1;
     } break;
case 59:
# line 404 "lua.stx"
{code_byte(POP); ntemp--;} break;
case 60:
# line 405 "lua.stx"
{ 
      *(yypvt[-2].pByte) = ONTJMP;
      *((Word *)(yypvt[-2].pByte+1)) = pc - (yypvt[-2].pByte + sizeof(Word)+1);
      yyval.vInt = 1;
     } break;
case 61:
# line 412 "lua.stx"
{ code_byte(PUSHNIL); incr_ntemp();} break;
case 63:
# line 416 "lua.stx"
{code_byte(PUSHMARK); yyval.vInt = ntemp; incr_ntemp();} break;
case 64:
# line 417 "lua.stx"
{ code_byte(CALLFUNC); ntemp = yypvt[-3].vInt-1;} break;
case 65:
# line 419 "lua.stx"
{lua_pushvar (yypvt[-0].vLong); } break;
case 66:
# line 422 "lua.stx"
{ yyval.vInt = 1; } break;
case 67:
# line 423 "lua.stx"
{ yyval.vInt = yypvt[-0].vInt; } break;
case 68:
# line 426 "lua.stx"
{ yyval.vInt = yypvt[-0].vInt; } break;
case 69:
# line 427 "lua.stx"
{if (!yypvt[-1].vInt){lua_codeadjust (ntemp+1); incr_ntemp();}} break;
case 70:
# line 428 "lua.stx"
{yyval.vInt = yypvt[-0].vInt;} break;
case 73:
# line 435 "lua.stx"
{localvar[nlocalvar]=yypvt[-0].vWord; incr_nlocalvar();} break;
case 74:
# line 436 "lua.stx"
{localvar[nlocalvar]=yypvt[-0].vWord; incr_nlocalvar();} break;
case 75:
# line 439 "lua.stx"
{yyval.vLong=-1;} break;
case 76:
# line 440 "lua.stx"
{yyval.vLong=yypvt[-0].vWord;} break;
case 77:
# line 443 "lua.stx"
{ yyval.vInt = yypvt[-1].vInt; } break;
case 78:
# line 444 "lua.stx"
{ yyval.vInt = yypvt[-1].vInt; } break;
case 79:
# line 447 "lua.stx"
{ yyval.vInt = 0; } break;
case 80:
# line 448 "lua.stx"
{ yyval.vInt = yypvt[-0].vInt; } break;
case 81:
# line 451 "lua.stx"
{yyval.vInt=1;} break;
case 82:
# line 452 "lua.stx"
{yyval.vInt=yypvt[-2].vInt+1;} break;
case 83:
# line 456 "lua.stx"
{
            align(Word); 
            code_byte(PUSHSTRING);
	    code_word(lua_findconstant (s_name(yypvt[-0].vWord)));
            incr_ntemp();
	   } break;
case 84:
# line 463 "lua.stx"
{
	    code_byte(STOREFIELD);
	    ntemp-=2;
	   } break;
case 85:
# line 469 "lua.stx"
{ yyval.vInt = 0; } break;
case 86:
# line 470 "lua.stx"
{ yyval.vInt = yypvt[-0].vInt; } break;
case 87:
# line 473 "lua.stx"
{ code_number(1); } break;
case 88:
# line 473 "lua.stx"
{yyval.vInt=1;} break;
case 89:
# line 474 "lua.stx"
{ code_number(yypvt[-1].vInt+1); } break;
case 90:
# line 475 "lua.stx"
{yyval.vInt=yypvt[-3].vInt+1;} break;
case 91:
# line 479 "lua.stx"
{
	    code_byte(STOREFIELD);
	    ntemp-=2;
	   } break;
case 92:
# line 486 "lua.stx"
{
	   nvarbuffer = 0; 
           varbuffer[nvarbuffer] = yypvt[-0].vLong; incr_nvarbuffer();
	   yyval.vInt = (yypvt[-0].vLong == 0) ? 1 : 0;
	  } break;
case 93:
# line 492 "lua.stx"
{ 
           varbuffer[nvarbuffer] = yypvt[-0].vLong; incr_nvarbuffer();
	   yyval.vInt = (yypvt[-0].vLong == 0) ? yypvt[-2].vInt + 1 : yypvt[-2].vInt;
	  } break;
case 94:
# line 499 "lua.stx"
{ 
	   int local = lua_localname (yypvt[-0].vWord);
	   if (local == -1)	/* global var */
	    yyval.vLong = yypvt[-0].vWord + 1;		/* return positive value */
           else
	    yyval.vLong = -(local+1);		/* return negative value */
	  } break;
case 95:
# line 507 "lua.stx"
{lua_pushvar (yypvt[-0].vLong);} break;
case 96:
# line 508 "lua.stx"
{
	   yyval.vLong = 0;		/* indexed variable */
	  } break;
case 97:
# line 511 "lua.stx"
{lua_pushvar (yypvt[-0].vLong);} break;
case 98:
# line 512 "lua.stx"
{
	   align(Word);
	   code_byte(PUSHSTRING);
	   code_word(lua_findconstant (s_name(yypvt[-0].vWord))); incr_ntemp();
	   yyval.vLong = 0;		/* indexed variable */
	  } break;
case 99:
# line 520 "lua.stx"
{localvar[nlocalvar]=yypvt[-1].vWord; incr_nlocalvar();} break;
case 100:
# line 521 "lua.stx"
{localvar[nlocalvar]=yypvt[-1].vWord; incr_nlocalvar();} break;
case 101:
# line 524 "lua.stx"
{ code_byte(PUSHNIL); } break;
case 102:
# line 525 "lua.stx"
{ntemp = 0;} break;
case 104:
# line 528 "lua.stx"
{lua_debug = yypvt[-0].vInt;} break;
	}
	goto yystack;		/* reset registers in driver code */
}
