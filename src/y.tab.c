
# line 2 "lua.stx"

char *rcs_luastx = "$Id: lua.stx,v 2.4 1994/04/20 16:22:21 celes Exp $";

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mm.h"

#include "opcode.h"
#include "hash.h"
#include "inout.h"
#include "table.h"
#include "lua.h"

#define LISTING 0

#ifndef GAPCODE
#define GAPCODE 50
#endif
static Word   maxcode;
static Word   maxmain;
static Word   maxcurr ;
static Byte  *code = NULL;
static Byte  *initcode;
static Byte  *basepc;
static Word   maincode;
static Word   pc;

#define MAXVAR 32
static long    varbuffer[MAXVAR];    /* variables in an assignment list;
				it's long to store negative Word values */
static int     nvarbuffer=0;	     /* number of variables at a list */

static Word    localvar[STACKGAP];   /* store local variable names */
static int     nlocalvar=0;	     /* number of local variables */

#define MAXFIELDS FIELDS_PER_FLUSH*2
static Word    fields[MAXFIELDS];     /* fieldnames to be flushed */
static int     nfields=0;
static int     ntemp;		     /* number of temporary var into stack */
static int     err;		     /* flag to indicate error */

/* Internal functions */

static void code_byte (Byte c)
{
 if (pc>maxcurr-2)  /* 1 byte free to code HALT of main code */
 {
  maxcurr += GAPCODE;
  basepc = (Byte *)realloc(basepc, maxcurr*sizeof(Byte));
  if (basepc == NULL)
  {
   lua_error ("not enough memory");
   err = 1;
  }
 }
 basepc[pc++] = c;
}

static void code_word (Word n)
{
 CodeWord code;
 code.w = n;
 code_byte(code.m.c1);
 code_byte(code.m.c2);
}

static void code_float (float n)
{
 CodeFloat code;
 code.f = n;
 code_byte(code.m.c1);
 code_byte(code.m.c2);
 code_byte(code.m.c3);
 code_byte(code.m.c4);
}

static void code_word_at (Byte *p, Word n)
{
 CodeWord code;
 code.w = n;
 *p++ = code.m.c1;
 *p++ = code.m.c2;
}

static void push_field (Word name)
{
  if (nfields < STACKGAP-1)
    fields[nfields++] = name;
  else
  {
   lua_error ("too many fields in a constructor");
   err = 1;
  }
}

static void flush_record (int n)
{
  int i;
  if (n == 0) return;
  code_byte(STORERECORD);
  code_byte(n);
  for (i=0; i<n; i++)
    code_word(fields[--nfields]);
  ntemp -= n;
}

static void flush_list (int m, int n)
{
  if (n == 0) return;
  if (m == 0)
    code_byte(STORELIST0); 
  else
  {
    code_byte(STORELIST);
    code_byte(m);
  }
  code_byte(n);
  ntemp-=n;
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

static void add_nlocalvar (int n)
{
 if (ntemp+nlocalvar+MAXVAR+n < STACKGAP)
  nlocalvar += n;
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

static void code_number (float f)
{ Word i = (Word)f;
  if (f == (float)i)  /* f has an (short) integer value */
  {
   if (i <= 2) code_byte(PUSH0 + i);
   else if (i <= 255)
   {
    code_byte(PUSHBYTE);
    code_byte(i);
   }
   else
   {
    code_byte(PUSHWORD);
    code_word(i);
   }
  }
  else
  {
   code_byte(PUSHFLOAT);
   code_float(f);
  }
  incr_ntemp();
}


# line 184 "lua.stx"
typedef union  
{
 int   vInt;
 long  vLong;
 float vFloat;
 char *pChar;
 Word  vWord;
 Byte *pByte;
} YYSTYPE;
# define WRONGTOKEN 257
# define NIL 258
# define IF 259
# define THEN 260
# define ELSE 261
# define ELSEIF 262
# define WHILE 263
# define DO 264
# define REPEAT 265
# define UNTIL 266
# define END 267
# define RETURN 268
# define LOCAL 269
# define NUMBER 270
# define FUNCTION 271
# define STRING 272
# define NAME 273
# define DEBUG 274
# define AND 275
# define OR 276
# define NE 277
# define LE 278
# define GE 279
# define CONC 280
# define UNARY 281
# define NOT 282
#define yyclearin yychar = -1
#define yyerrok yyerrflag = 0
extern int yychar;
extern int yyerrflag;
#ifndef YYMAXDEPTH
#define YYMAXDEPTH 150
#endif
YYSTYPE yylval, yyval;
# define YYERRCODE 256

# line 622 "lua.stx"


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
 Byte *init = initcode = (Byte *) calloc(GAPCODE, sizeof(Byte));
 maincode = 0; 
 maxmain = GAPCODE;
 if (init == NULL)
 {
  lua_error("not enough memory");
  return 1;
 }
 err = 0;
 if (yyparse () || (err==1)) return 1;
 initcode[maincode++] = HALT;
 init = initcode;
#if LISTING
 PrintCode(init,init+maincode);
#endif
 if (lua_execute (init)) return 1;
 free(init);
 return 0;
}


#if LISTING

static void PrintCode (Byte *code, Byte *end)
{
 Byte *p = code;
 printf ("\n\nCODE\n");
 while (p != end)
 {
  switch ((OpCode)*p)
  {
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
                        {
			 CodeWord c;
			 int n = p-code;
			 p++;
			 get_word(c,p);
    			 printf ("%d    PUSHWORD   %d\n", n, c.w);
			}
   			break;
   case PUSHFLOAT:
			{
			 CodeFloat c;
			 int n = p-code;
			 p++;
			 get_float(c,p);
    			 printf ("%d    PUSHFLOAT  %f\n", n, c.f);
			}
   			break;
   case PUSHSTRING:
    			{
			 CodeWord c;
			 int n = p-code;
			 p++;
			 get_word(c,p);
    			 printf ("%d    PUSHSTRING   %d\n", n, c.w);
			}
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
    			{
			 CodeWord c;
			 int n = p-code;
			 p++;
			 get_word(c,p);
    			 printf ("%d    PUSHGLOBAL   %d\n", n, c.w);
			}
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
    			printf ("%d    STORELOCAL   %d\n", p-code, *(++p));
    			p++;
   			break;
   case STOREGLOBAL:
    			{
			 CodeWord c;
			 int n = p-code;
			 p++;
			 get_word(c,p);
    			 printf ("%d    STOREGLOBAL   %d\n", n, c.w);
			}
   			break;
   case STOREINDEXED0:  printf ("%d    STOREINDEXED0\n", (p++)-code); break;
   case STOREINDEXED:   printf ("%d    STOREINDEXED   %d\n", p-code, *(++p));
    			p++;
   			break;
   case STORELIST0:
	printf("%d      STORELIST0  %d\n", p-code, *(++p));
        p++;
        break;
   case STORELIST:
	printf("%d      STORELIST  %d %d\n", p-code, *(p+1), *(p+2));
        p+=3;
        break;
   case STORERECORD:
       printf("%d      STORERECORD  %d\n", p-code, *(++p));
       p += *p*sizeof(Word) + 1;
       break;
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
    			{
			 CodeWord c;
			 int n = p-code;
			 p++;
			 get_word(c,p);
    			 printf ("%d    ONTJMP  %d\n", n, c.w);
			}
   			break;
   case ONFJMP:	   
    			{
			 CodeWord c;
			 int n = p-code;
			 p++;
			 get_word(c,p);
    			 printf ("%d    ONFJMP  %d\n", n, c.w);
			}
   			break;
   case JMP:	   
    			{
			 CodeWord c;
			 int n = p-code;
			 p++;
			 get_word(c,p);
    			 printf ("%d    JMP  %d\n", n, c.w);
			}
   			break;
   case UPJMP:
    			{
			 CodeWord c;
			 int n = p-code;
			 p++;
			 get_word(c,p);
    			 printf ("%d    UPJMP  %d\n", n, c.w);
			}
   			break;
   case IFFJMP:
    			{
			 CodeWord c;
			 int n = p-code;
			 p++;
			 get_word(c,p);
    			 printf ("%d    IFFJMP  %d\n", n, c.w);
			}
   			break;
   case IFFUPJMP:
    			{
			 CodeWord c;
			 int n = p-code;
			 p++;
			 get_word(c,p);
    			 printf ("%d    IFFUPJMP  %d\n", n, c.w);
			}
   			break;
   case POP:       	printf ("%d    POP\n", (p++)-code); break;
   case CALLFUNC:	printf ("%d    CALLFUNC\n", (p++)-code); break;
   case RETCODE:
    			printf ("%d    RETCODE   %d\n", p-code, *(++p));
    			p++;
   			break;
   case HALT:		printf ("%d    HALT\n", (p++)-code); break;
   case SETFUNCTION:
                        {
                         CodeWord c1, c2;
                         int n = p-code;
                         p++;
                         get_word(c1,p);
                         get_word(c2,p);
                         printf ("%d    SETFUNCTION  %d  %d\n", n, c1.w, c2.w);
                        }
                        break;
   case SETLINE:
                        {
                         CodeWord c;
                         int n = p-code;
                         p++;
                         get_word(c,p);
                         printf ("%d    SETLINE  %d\n", n, c.w);
                        }
                        break;

   case RESET:		printf ("%d    RESET\n", (p++)-code); break;
   default:		printf ("%d    Cannot happen: code %d\n", (p++)-code, *(p-1)); break;
  }
 }
}
#endif

int yyexca[] ={
-1, 1,
	0, -1,
	-2, 2,
-1, 20,
	40, 67,
	91, 94,
	46, 96,
	-2, 91,
-1, 32,
	40, 67,
	91, 94,
	46, 96,
	-2, 51,
-1, 73,
	275, 34,
	276, 34,
	61, 34,
	277, 34,
	62, 34,
	60, 34,
	278, 34,
	279, 34,
	280, 34,
	43, 34,
	45, 34,
	42, 34,
	47, 34,
	-2, 70,
-1, 74,
	91, 94,
	46, 96,
	-2, 92,
-1, 105,
	261, 28,
	262, 28,
	266, 28,
	267, 28,
	268, 28,
	-2, 11,
-1, 125,
	268, 31,
	-2, 30,
-1, 146,
	275, 34,
	276, 34,
	61, 34,
	277, 34,
	62, 34,
	60, 34,
	278, 34,
	279, 34,
	280, 34,
	43, 34,
	45, 34,
	42, 34,
	47, 34,
	-2, 72,
	};
# define YYNPROD 103
# define YYLAST 364
int yyact[]={

    58,    56,    22,    57,   132,    59,    58,    56,   137,    57,
   110,    59,    58,    56,   107,    57,    85,    59,    51,    50,
    52,    82,    23,    43,    51,    50,    52,    58,    56,     9,
    57,   157,    59,    58,    56,   165,    57,     5,    59,   162,
     6,   161,   104,   154,   155,    51,    50,    52,    64,   153,
    70,    51,    50,    52,    26,    58,    56,   127,    57,    10,
    59,   111,    25,    78,    27,    58,    56,    28,    57,    29,
    59,   131,   147,    51,    50,    52,     7,    65,    66,   115,
   150,   112,    63,    51,    50,    52,    68,    69,    31,   159,
    11,    79,    58,    76,   128,    73,    41,    59,   151,    87,
    88,    89,    90,    91,    92,    93,    94,    95,    96,    97,
    77,   114,   148,    40,    58,    56,   102,    57,   106,    59,
   117,    32,    72,   121,   116,   100,    80,   109,    67,    48,
    20,    36,    73,    30,    45,    73,    44,   118,   149,    84,
    17,   126,    18,    46,    21,    47,   120,   119,   101,   145,
   144,   125,    71,   123,    75,    39,    38,    12,     8,   108,
   105,   136,    83,    74,   135,    24,     4,     3,   139,   140,
     2,    81,   134,   141,   133,   130,   129,    42,   113,    16,
     1,   146,   124,     0,   143,     0,     0,   152,     0,     0,
     0,    86,     0,     0,     0,     0,     0,    13,     0,     0,
   160,    14,     0,    15,   164,   163,     0,    19,   167,     0,
     0,    23,    73,     0,     0,     0,     0,     0,   168,   166,
   158,   171,   173,     0,     0,     0,   169,     0,     0,     0,
     0,     0,     0,    61,    62,    53,    54,    55,    60,    61,
    62,    53,    54,    55,    60,     0,     0,     0,     0,   103,
    60,    49,     0,    98,    99,     0,     0,     0,     0,     0,
    61,    62,    53,    54,    55,    60,    61,    62,    53,    54,
    55,    60,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,    35,     0,     0,     0,     0,     0,    61,    62,
    53,    54,    55,    60,    33,   122,    34,    23,     0,     0,
    53,    54,    55,    60,     0,     0,    37,     0,     0,     0,
   138,     0,     0,     0,     0,   142,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,   156,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
   170,     0,     0,   172 };
int yypact[]={

 -1000,  -234, -1000, -1000, -1000,  -244, -1000,    31,   -62, -1000,
 -1000, -1000, -1000,    24, -1000, -1000,    52, -1000, -1000,  -250,
 -1000, -1000, -1000, -1000,    89,    -9, -1000,    24,    24,    24,
 -1000,    88, -1000, -1000, -1000, -1000, -1000,    24,    24, -1000,
    24,  -251,    49, -1000,   -28,    45,    86,  -252,  -257, -1000,
    24,    24,    24,    24,    24,    24,    24,    24,    24,    24,
    24, -1000, -1000,    84,    13, -1000, -1000,    24, -1000,   -15,
  -224, -1000,    74, -1000, -1000, -1000,  -259,    24,    24,  -263,
    24,   -12, -1000,    83,    76, -1000, -1000,   -30,   -30,   -30,
   -30,   -30,   -30,    50,    50, -1000, -1000,    72, -1000, -1000,
 -1000,    82,    13, -1000,    24, -1000, -1000, -1000,    74,   -36,
 -1000,    53,    74, -1000,  -269,    24, -1000,  -265, -1000,    24,
    24, -1000, -1000,    13,    31, -1000,    24, -1000, -1000,   -53,
    68, -1000, -1000,   -13,    54,    13, -1000, -1000,  -218,    23,
    23, -1000, -1000, -1000, -1000,  -237, -1000, -1000,  -269,    28,
 -1000,    24,  -226,  -228, -1000,    24,  -232,    24, -1000,    24,
    13, -1000, -1000, -1000,   -42, -1000,    31,    13, -1000, -1000,
 -1000, -1000,  -218, -1000 };
int yypgo[]={

     0,   180,   191,    54,    61,    81,   179,   133,   178,   177,
   176,   175,   174,   172,   121,   171,   170,    76,    59,   167,
   166,   165,   162,   161,    50,   160,   158,   157,    48,    49,
   156,   155,   131,   154,   152,   151,   150,   149,   148,   147,
   146,   145,   144,   143,   141,   139,    71,   138,   136,   134 };
int yyr1[]={

     0,     1,    16,     1,     1,     1,    21,    23,    19,    25,
    25,    26,    17,    18,    18,    27,    30,    27,    31,    27,
    27,    27,    27,    27,    29,    29,    29,    34,    35,    24,
    36,    37,    36,     2,    28,     3,     3,     3,     3,     3,
     3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
     3,     3,     3,     3,     3,     3,     3,    39,     3,    40,
     3,    41,     7,    38,    38,    43,    32,    42,     4,     4,
     5,    44,     5,    22,    22,    45,    45,    15,    15,     8,
     8,    10,    10,    11,    11,    47,    46,    12,    12,    13,
    13,     6,     6,    14,    48,    14,    49,    14,     9,     9,
    33,    33,    20 };
int yyr2[]={

     0,     0,     1,     9,     4,     4,     1,     1,    19,     0,
     6,     1,     4,     0,     2,    17,     1,    17,     1,    13,
     7,     3,     3,     7,     0,     4,    15,     1,     1,     9,
     0,     1,     9,     1,     3,     7,     7,     7,     7,     7,
     7,     7,     7,     7,     7,     7,     7,     5,     5,     3,
     9,     3,     3,     3,     3,     3,     5,     1,    11,     1,
    11,     1,     9,     1,     2,     1,    11,     3,     1,     3,
     3,     1,     9,     0,     2,     3,     7,     1,     3,     7,
     7,     1,     3,     3,     7,     1,     9,     1,     3,     3,
     7,     3,     7,     3,     1,    11,     1,     9,     3,     7,
     0,     4,     3 };
int yychk[]={

 -1000,    -1,   -16,   -19,   -20,   271,   274,   -17,   -26,   273,
   -18,    59,   -27,   259,   263,   265,    -6,   -32,    -7,   269,
   -14,   -42,    64,   273,   -21,   -28,    -3,    40,    43,    45,
    -7,    64,   -14,   270,   272,   258,   -32,   282,   -30,   -31,
    61,    44,    -9,   273,   -48,   -49,   -43,   -41,    40,   260,
    61,    60,    62,   277,   278,   279,    43,    45,    42,    47,
   280,   275,   276,    -3,   -28,   -28,   -28,    40,   -28,   -28,
   -24,   -34,    -5,    -3,   -14,   -33,    44,    61,    91,    46,
    40,   -15,   273,   -22,   -45,   273,    -2,   -28,   -28,   -28,
   -28,   -28,   -28,   -28,   -28,   -28,   -28,   -28,    -2,    -2,
    41,   -38,   -28,   264,   266,   -25,    44,   273,    -5,   -28,
   273,    -4,    -5,    -8,   123,    91,    41,    44,   -24,   -39,
   -40,    41,    -2,   -28,   -17,   -35,   -44,    93,    41,   -10,
   -11,   -46,   273,   -12,   -13,   -28,   -23,   273,    -2,   -28,
   -28,   -24,    -2,   -18,   -36,   -37,    -3,   125,    44,   -47,
    93,    44,   -24,   -29,   261,   262,    -2,   268,   -46,    61,
   -28,   267,   267,   -24,   -28,   267,    -4,   -28,   260,   -18,
    -2,   -24,    -2,   -29 };
int yydef[]={

     1,    -2,    11,     4,     5,     0,   102,    13,     0,     6,
     3,    14,    12,     0,    16,    18,     0,    21,    22,     0,
    -2,    65,    61,    93,     0,     0,    34,     0,     0,     0,
    49,    61,    -2,    52,    53,    54,    55,     0,     0,    27,
     0,     0,   100,    98,     0,     0,     0,    77,    73,    33,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,    33,    33,    34,     0,    47,    48,    63,    56,     0,
     0,     9,    20,    -2,    -2,    23,     0,     0,     0,     0,
    68,     0,    78,     0,    74,    75,    27,    36,    37,    38,
    39,    40,    41,    42,    43,    44,    45,    46,    57,    59,
    35,     0,    64,    33,     0,    -2,    71,    99,   101,     0,
    97,     0,    69,    62,    81,    87,     7,     0,    33,     0,
     0,    50,    27,    33,    13,    -2,     0,    95,    66,     0,
    82,    83,    85,     0,    88,    89,    27,    76,    24,    58,
    60,    33,    19,    10,    29,     0,    -2,    79,     0,     0,
    80,     0,     0,     0,    27,     0,     0,    68,    84,     0,
    90,     8,    15,    25,     0,    17,    13,    86,    33,    32,
    27,    33,    24,    26 };
typedef struct { char *t_name; int t_val; } yytoktype;
#ifndef YYDEBUG
#	define YYDEBUG	0	/* don't allow debugging */
#endif

#if YYDEBUG

yytoktype yytoks[] =
{
	"WRONGTOKEN",	257,
	"NIL",	258,
	"IF",	259,
	"THEN",	260,
	"ELSE",	261,
	"ELSEIF",	262,
	"WHILE",	263,
	"DO",	264,
	"REPEAT",	265,
	"UNTIL",	266,
	"END",	267,
	"RETURN",	268,
	"LOCAL",	269,
	"NUMBER",	270,
	"FUNCTION",	271,
	"STRING",	272,
	"NAME",	273,
	"DEBUG",	274,
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
	"UNARY",	281,
	"NOT",	282,
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
	"stat1 : typeconstructor",
	"stat1 : LOCAL localdeclist decinit",
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
	"expr : typeconstructor",
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
	"typeconstructor : '@'",
	"typeconstructor : '@' objectname fieldlist",
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
	"lfieldlist1 : expr1",
	"lfieldlist1 : lfieldlist1 ',' expr1",
	"varlist1 : var",
	"varlist1 : varlist1 ',' var",
	"var : NAME",
	"var : var",
	"var : var '[' expr1 ']'",
	"var : var",
	"var : var '.' NAME",
	"localdeclist : NAME",
	"localdeclist : localdeclist ',' NAME",
	"decinit : /* empty */",
	"decinit : '=' exprlist1",
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
# line 227 "lua.stx"
{
	  	  pc=maincode; basepc=initcode; maxcurr=maxmain;
		  nlocalvar=0;
	        } break;
case 3:
# line 232 "lua.stx"
{
		  maincode=pc; initcode=basepc; maxmain=maxcurr;
		} break;
case 6:
# line 240 "lua.stx"
{
		if (code == NULL)	/* first function */
		{
		 code = (Byte *) calloc(GAPCODE, sizeof(Byte));
		 if (code == NULL)
		 {
		  lua_error("not enough memory");
		  err = 1;
		 }
		 maxcode = GAPCODE;
		}
		pc=0; basepc=code; maxcurr=maxcode; 
		nlocalvar=0;
		yyval.vWord = lua_findsymbol(yypvt[-0].pChar); 
	       } break;
case 7:
# line 256 "lua.stx"
{
	        if (lua_debug)
		{
	         code_byte(SETFUNCTION); 
                 code_word(lua_nfile-1);
		 code_word(yypvt[-3].vWord);
		}
	        lua_codeadjust (0);
	       } break;
case 8:
# line 267 "lua.stx"
{ 
                if (lua_debug) code_byte(RESET); 
	        code_byte(RETCODE); code_byte(nlocalvar);
	        s_tag(yypvt[-6].vWord) = T_FUNCTION;
	        s_bvalue(yypvt[-6].vWord) = calloc (pc, sizeof(Byte));
		if (s_bvalue(yypvt[-6].vWord) == NULL)
		{
		 lua_error("not enough memory");
		 err = 1;
		}
	        memcpy (s_bvalue(yypvt[-6].vWord), basepc, pc*sizeof(Byte));
		code = basepc; maxcode=maxcurr;
#if LISTING
PrintCode(code,code+pc);
#endif
	       } break;
case 11:
# line 289 "lua.stx"
{
            ntemp = 0; 
            if (lua_debug)
            {
             code_byte(SETLINE); code_word(lua_linenumber);
            }
	   } break;
case 15:
# line 302 "lua.stx"
{
        {
	 Word elseinit = yypvt[-2].vWord+sizeof(Word)+1;
	 if (pc - elseinit == 0)		/* no else */
	 {
	  pc -= sizeof(Word)+1;
	  elseinit = pc;
	 }
	 else
	 {
	  basepc[yypvt[-2].vWord] = JMP;
	  code_word_at(basepc+yypvt[-2].vWord+1, pc - elseinit);
	 }
	 basepc[yypvt[-4].vWord] = IFFJMP;
	 code_word_at(basepc+yypvt[-4].vWord+1,elseinit-(yypvt[-4].vWord+sizeof(Word)+1));
	}
       } break;
case 16:
# line 320 "lua.stx"
{yyval.vWord=pc;} break;
case 17:
# line 322 "lua.stx"
{
        basepc[yypvt[-3].vWord] = IFFJMP;
	code_word_at(basepc+yypvt[-3].vWord+1, pc - (yypvt[-3].vWord + sizeof(Word)+1));
        
        basepc[yypvt[-1].vWord] = UPJMP;
	code_word_at(basepc+yypvt[-1].vWord+1, pc - (yypvt[-6].vWord));
       } break;
case 18:
# line 330 "lua.stx"
{yyval.vWord=pc;} break;
case 19:
# line 332 "lua.stx"
{
        basepc[yypvt[-0].vWord] = IFFUPJMP;
	code_word_at(basepc+yypvt[-0].vWord+1, pc - (yypvt[-4].vWord));
       } break;
case 20:
# line 339 "lua.stx"
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
# line 350 "lua.stx"
{ lua_codeadjust (0); } break;
case 22:
# line 351 "lua.stx"
{ lua_codeadjust (0); } break;
case 23:
# line 352 "lua.stx"
{ add_nlocalvar(yypvt[-1].vInt); lua_codeadjust (0); } break;
case 26:
# line 358 "lua.stx"
{
          {
  	   Word elseinit = yypvt[-1].vWord+sizeof(Word)+1;
  	   if (pc - elseinit == 0)		/* no else */
  	   {
  	    pc -= sizeof(Word)+1;
	    elseinit = pc;
	   }
	   else
	   {
	    basepc[yypvt[-1].vWord] = JMP;
	    code_word_at(basepc+yypvt[-1].vWord+1, pc - elseinit);
	   }
	   basepc[yypvt[-3].vWord] = IFFJMP;
	   code_word_at(basepc+yypvt[-3].vWord+1, elseinit - (yypvt[-3].vWord + sizeof(Word)+1));
	  }  
         } break;
case 27:
# line 377 "lua.stx"
{yyval.vInt = nlocalvar;} break;
case 28:
# line 377 "lua.stx"
{ntemp = 0;} break;
case 29:
# line 378 "lua.stx"
{
	  if (nlocalvar != yypvt[-3].vInt)
	  {
           nlocalvar = yypvt[-3].vInt;
	   lua_codeadjust (0);
	  }
         } break;
case 31:
# line 388 "lua.stx"
{ if (lua_debug){code_byte(SETLINE);code_word(lua_linenumber);}} break;
case 32:
# line 390 "lua.stx"
{ 
           if (lua_debug) code_byte(RESET); 
           code_byte(RETCODE); code_byte(nlocalvar);
          } break;
case 33:
# line 397 "lua.stx"
{ 
	  yyval.vWord = pc;
	  code_byte(0);		/* open space */
	  code_word (0);
         } break;
case 34:
# line 403 "lua.stx"
{ if (yypvt[-0].vInt == 0) {lua_codeadjust (ntemp+1); incr_ntemp();}} break;
case 35:
# line 406 "lua.stx"
{ yyval.vInt = yypvt[-1].vInt; } break;
case 36:
# line 407 "lua.stx"
{ code_byte(EQOP);   yyval.vInt = 1; ntemp--;} break;
case 37:
# line 408 "lua.stx"
{ code_byte(LTOP);   yyval.vInt = 1; ntemp--;} break;
case 38:
# line 409 "lua.stx"
{ code_byte(LEOP); code_byte(NOTOP); yyval.vInt = 1; ntemp--;} break;
case 39:
# line 410 "lua.stx"
{ code_byte(EQOP); code_byte(NOTOP); yyval.vInt = 1; ntemp--;} break;
case 40:
# line 411 "lua.stx"
{ code_byte(LEOP);   yyval.vInt = 1; ntemp--;} break;
case 41:
# line 412 "lua.stx"
{ code_byte(LTOP); code_byte(NOTOP); yyval.vInt = 1; ntemp--;} break;
case 42:
# line 413 "lua.stx"
{ code_byte(ADDOP);  yyval.vInt = 1; ntemp--;} break;
case 43:
# line 414 "lua.stx"
{ code_byte(SUBOP);  yyval.vInt = 1; ntemp--;} break;
case 44:
# line 415 "lua.stx"
{ code_byte(MULTOP); yyval.vInt = 1; ntemp--;} break;
case 45:
# line 416 "lua.stx"
{ code_byte(DIVOP);  yyval.vInt = 1; ntemp--;} break;
case 46:
# line 417 "lua.stx"
{ code_byte(CONCOP);  yyval.vInt = 1; ntemp--;} break;
case 47:
# line 418 "lua.stx"
{ yyval.vInt = 1; } break;
case 48:
# line 419 "lua.stx"
{ code_byte(MINUSOP); yyval.vInt = 1;} break;
case 49:
# line 420 "lua.stx"
{ yyval.vInt = yypvt[-0].vInt; } break;
case 50:
# line 422 "lua.stx"
{ 
      code_byte(CREATEARRAY);
      yyval.vInt = 1;
     } break;
case 51:
# line 426 "lua.stx"
{ lua_pushvar (yypvt[-0].vLong); yyval.vInt = 1;} break;
case 52:
# line 427 "lua.stx"
{ code_number(yypvt[-0].vFloat); yyval.vInt = 1; } break;
case 53:
# line 429 "lua.stx"
{
      code_byte(PUSHSTRING);
      code_word(yypvt[-0].vWord);
      yyval.vInt = 1;
      incr_ntemp();
     } break;
case 54:
# line 435 "lua.stx"
{code_byte(PUSHNIL); yyval.vInt = 1; incr_ntemp();} break;
case 55:
# line 437 "lua.stx"
{
      yyval.vInt = 0;
      if (lua_debug)
      {
       code_byte(SETLINE); code_word(lua_linenumber);
      }
     } break;
case 56:
# line 444 "lua.stx"
{ code_byte(NOTOP);  yyval.vInt = 1;} break;
case 57:
# line 445 "lua.stx"
{code_byte(POP); ntemp--;} break;
case 58:
# line 446 "lua.stx"
{ 
      basepc[yypvt[-2].vWord] = ONFJMP;
      code_word_at(basepc+yypvt[-2].vWord+1, pc - (yypvt[-2].vWord + sizeof(Word)+1));
      yyval.vInt = 1;
     } break;
case 59:
# line 451 "lua.stx"
{code_byte(POP); ntemp--;} break;
case 60:
# line 452 "lua.stx"
{ 
      basepc[yypvt[-2].vWord] = ONTJMP;
      code_word_at(basepc+yypvt[-2].vWord+1, pc - (yypvt[-2].vWord + sizeof(Word)+1));
      yyval.vInt = 1;
     } break;
case 61:
# line 460 "lua.stx"
{
      code_byte(PUSHBYTE);
      yyval.vWord = pc; code_byte(0);
      incr_ntemp();
      code_byte(CREATEARRAY);
     } break;
case 62:
# line 467 "lua.stx"
{
      basepc[yypvt[-2].vWord] = yypvt[-0].vInt; 
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
        code_byte(SETLINE); code_word(lua_linenumber);
       }
      }
     } break;
case 63:
# line 491 "lua.stx"
{ code_byte(PUSHNIL); incr_ntemp();} break;
case 65:
# line 495 "lua.stx"
{code_byte(PUSHMARK); yyval.vInt = ntemp; incr_ntemp();} break;
case 66:
# line 496 "lua.stx"
{ code_byte(CALLFUNC); ntemp = yypvt[-3].vInt-1;} break;
case 67:
# line 498 "lua.stx"
{lua_pushvar (yypvt[-0].vLong); } break;
case 68:
# line 501 "lua.stx"
{ yyval.vInt = 1; } break;
case 69:
# line 502 "lua.stx"
{ yyval.vInt = yypvt[-0].vInt; } break;
case 70:
# line 505 "lua.stx"
{ yyval.vInt = yypvt[-0].vInt; } break;
case 71:
# line 506 "lua.stx"
{if (!yypvt[-1].vInt){lua_codeadjust (ntemp+1); incr_ntemp();}} break;
case 72:
# line 507 "lua.stx"
{yyval.vInt = yypvt[-0].vInt;} break;
case 75:
# line 515 "lua.stx"
{
		 localvar[nlocalvar]=lua_findsymbol(yypvt[-0].pChar); 
		 add_nlocalvar(1);
		} break;
case 76:
# line 520 "lua.stx"
{
		 localvar[nlocalvar]=lua_findsymbol(yypvt[-0].pChar); 
		 add_nlocalvar(1);
		} break;
case 77:
# line 526 "lua.stx"
{yyval.vLong=-1;} break;
case 78:
# line 527 "lua.stx"
{yyval.vLong=lua_findsymbol(yypvt[-0].pChar);} break;
case 79:
# line 531 "lua.stx"
{ 
	       flush_record(yypvt[-1].vInt%FIELDS_PER_FLUSH); 
	       yyval.vInt = yypvt[-1].vInt;
	      } break;
case 80:
# line 536 "lua.stx"
{ 
	       flush_list(yypvt[-1].vInt/FIELDS_PER_FLUSH, yypvt[-1].vInt%FIELDS_PER_FLUSH);
	       yyval.vInt = yypvt[-1].vInt;
     	      } break;
case 81:
# line 542 "lua.stx"
{ yyval.vInt = 0; } break;
case 82:
# line 543 "lua.stx"
{ yyval.vInt = yypvt[-0].vInt; } break;
case 83:
# line 546 "lua.stx"
{yyval.vInt=1;} break;
case 84:
# line 548 "lua.stx"
{
		  yyval.vInt=yypvt[-2].vInt+1;
		  if (yyval.vInt%FIELDS_PER_FLUSH == 0) flush_record(FIELDS_PER_FLUSH);
		} break;
case 85:
# line 554 "lua.stx"
{yyval.vWord = lua_findconstant(yypvt[-0].pChar);} break;
case 86:
# line 555 "lua.stx"
{ 
	       push_field(yypvt[-2].vWord);
	      } break;
case 87:
# line 560 "lua.stx"
{ yyval.vInt = 0; } break;
case 88:
# line 561 "lua.stx"
{ yyval.vInt = yypvt[-0].vInt; } break;
case 89:
# line 564 "lua.stx"
{yyval.vInt=1;} break;
case 90:
# line 566 "lua.stx"
{
		  yyval.vInt=yypvt[-2].vInt+1;
		  if (yyval.vInt%FIELDS_PER_FLUSH == 0) 
		    flush_list(yyval.vInt/FIELDS_PER_FLUSH - 1, FIELDS_PER_FLUSH);
		} break;
case 91:
# line 574 "lua.stx"
{
	   nvarbuffer = 0; 
           varbuffer[nvarbuffer] = yypvt[-0].vLong; incr_nvarbuffer();
	   yyval.vInt = (yypvt[-0].vLong == 0) ? 1 : 0;
	  } break;
case 92:
# line 580 "lua.stx"
{ 
           varbuffer[nvarbuffer] = yypvt[-0].vLong; incr_nvarbuffer();
	   yyval.vInt = (yypvt[-0].vLong == 0) ? yypvt[-2].vInt + 1 : yypvt[-2].vInt;
	  } break;
case 93:
# line 587 "lua.stx"
{
	   Word s = lua_findsymbol(yypvt[-0].pChar);
	   int local = lua_localname (s);
	   if (local == -1)	/* global var */
	    yyval.vLong = s + 1;		/* return positive value */
           else
	    yyval.vLong = -(local+1);		/* return negative value */
	  } break;
case 94:
# line 596 "lua.stx"
{lua_pushvar (yypvt[-0].vLong);} break;
case 95:
# line 597 "lua.stx"
{
	   yyval.vLong = 0;		/* indexed variable */
	  } break;
case 96:
# line 600 "lua.stx"
{lua_pushvar (yypvt[-0].vLong);} break;
case 97:
# line 601 "lua.stx"
{
	   code_byte(PUSHSTRING);
	   code_word(lua_findconstant(yypvt[-0].pChar)); incr_ntemp();
	   yyval.vLong = 0;		/* indexed variable */
	  } break;
case 98:
# line 608 "lua.stx"
{localvar[nlocalvar]=lua_findsymbol(yypvt[-0].pChar); yyval.vInt = 1;} break;
case 99:
# line 610 "lua.stx"
{
	     localvar[nlocalvar+yypvt[-2].vInt]=lua_findsymbol(yypvt[-0].pChar); 
	     yyval.vInt = yypvt[-2].vInt+1;
	    } break;
case 102:
# line 620 "lua.stx"
{lua_debug = yypvt[-0].vInt;} break;
	}
	goto yystack;		/* reset registers in driver code */
}
