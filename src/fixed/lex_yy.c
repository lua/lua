# include "stdio.h"
# define U(x) x
# define NLSTATE yyprevious=YYNEWLINE
# define BEGIN yybgin = yysvec + 1 +
# define INITIAL 0
# define YYLERR yysvec
# define YYSTATE (yyestate-yysvec-1)
# define YYOPTIM 1
# define YYLMAX BUFSIZ
# define output(c) putc(c,yyout)
# define input() (((yytchar=yysptr>yysbuf?U(*--yysptr):getc(yyin))==10?(yylineno++,yytchar):yytchar)==EOF?0:yytchar)
# define unput(c) {yytchar= (c);if(yytchar=='\n')yylineno--;*yysptr++=yytchar;}
# define yymore() (yymorfg=1)
# define ECHO fprintf(yyout, "%s",yytext)
# define REJECT { nstr = yyreject(); goto yyfussy;}
int yyleng; extern char yytext[];
int yymorfg;
extern char *yysptr, yysbuf[];
int yytchar;
FILE *yyin = {NULL}, *yyout = {NULL};
extern int yylineno;
struct yysvf { 
	struct yywork *yystoff;
	struct yysvf *yyother;
	int *yystops;};
struct yysvf *yyestate;
extern struct yysvf yysvec[], *yybgin;
#include <stdlib.h>
#include <string.h>

#include "opcode.h"
#include "hash.h"
#include "inout.h"
#include "table.h"
#include "y_tab.h"

#undef input
#undef unput

static Input input;
static Unput unput;

void lua_setinput (Input fn)
{
 input = fn;
}

void lua_setunput (Unput fn)
{
 unput = fn;
}

char *lua_lasttext (void)
{
 return yytext;
}

# define YYNEWLINE 10
yylex(){
int nstr; extern int yyprevious;
while((nstr = yylook()) >= 0)
yyfussy: switch(nstr){
case 0:
if(yywrap()) return(0); break;
case 1:
				;
break;
case 2:
			{yylval.vInt = 1; return DEBUG;}
break;
case 3:
			{yylval.vInt = 0; return DEBUG;}
break;
case 4:
				lua_linenumber++;
break;
case 5:
				;
break;
case 6:
				return LOCAL;
break;
case 7:
				return IF;
break;
case 8:
				return THEN;
break;
case 9:
				return ELSE;
break;
case 10:
			return ELSEIF;
break;
case 11:
				return WHILE;
break;
case 12:
				return DO;
break;
case 13:
			return REPEAT;
break;
case 14:
				return UNTIL;
break;
case 15:
			{
                                         yylval.vWord = lua_nfile-1;
                                         return FUNCTION;
					}
break;
case 16:
				return END;
break;
case 17:
				return RETURN;
break;
case 18:
				return LOCAL;
break;
case 19:
				return NIL;
break;
case 20:
				return AND;
break;
case 21:
				return OR;
break;
case 22:
				return NOT;
break;
case 23:
				return NE;
break;
case 24:
				return LE;
break;
case 25:
				return GE;
break;
case 26:
				return CONC;
break;
case 27:
			case 28:
		      {
				       yylval.vWord = lua_findenclosedconstant (yytext);
				       return STRING;
				      }
break;
case 29:
case 30:
case 31:
case 32:
{
				        yylval.vFloat = atof(yytext);
				        return NUMBER;
				       }
break;
case 33:
 	       {
					yylval.vWord = lua_findsymbol (yytext);
					return NAME;
				       }
break;
case 34:
				return  *yytext;
break;
case -1:
break;
default:
fprintf(yyout,"bad switch yylook %d",nstr);
} return(0); }
/* end of yylex */
int yyvstop[] = {
0,

1,
0,

1,
0,

34,
0,

1,
34,
0,

4,
0,

34,
0,

34,
0,

34,
0,

34,
0,

29,
34,
0,

34,
0,

34,
0,

33,
34,
0,

33,
34,
0,

33,
34,
0,

33,
34,
0,

33,
34,
0,

33,
34,
0,

33,
34,
0,

33,
34,
0,

33,
34,
0,

33,
34,
0,

33,
34,
0,

33,
34,
0,

33,
34,
0,

34,
0,

34,
0,

1,
0,

27,
0,

28,
0,

5,
0,

26,
0,

30,
0,

29,
0,

29,
0,

24,
0,

25,
0,

33,
0,

33,
0,

12,
33,
0,

33,
0,

33,
0,

33,
0,

7,
33,
0,

33,
0,

33,
0,

33,
0,

21,
33,
0,

33,
0,

33,
0,

33,
0,

33,
0,

23,
0,

29,
30,
0,

31,
0,

20,
33,
0,

33,
0,

16,
33,
0,

33,
0,

33,
0,

19,
33,
0,

22,
33,
0,

33,
0,

33,
0,

33,
0,

33,
0,

33,
0,

32,
0,

9,
33,
0,

33,
0,

33,
0,

33,
0,

33,
0,

8,
33,
0,

33,
0,

33,
0,

31,
32,
0,

33,
0,

33,
0,

6,
18,
33,
0,

33,
0,

33,
0,

14,
33,
0,

11,
33,
0,

10,
33,
0,

33,
0,

13,
33,
0,

17,
33,
0,

2,
0,

33,
0,

15,
33,
0,

3,
0,
0};
# define YYTYPE char
struct yywork { YYTYPE verify, advance; } yycrank[] = {
0,0,	0,0,	1,3,	0,0,	
0,0,	0,0,	0,0,	0,0,	
0,0,	0,0,	1,4,	1,5,	
6,29,	4,28,	0,0,	0,0,	
0,0,	0,0,	7,31,	0,0,	
6,29,	6,29,	0,0,	0,0,	
0,0,	0,0,	7,31,	7,31,	
0,0,	0,0,	0,0,	0,0,	
0,0,	0,0,	0,0,	1,6,	
4,28,	0,0,	0,0,	0,0,	
1,7,	0,0,	0,0,	0,0,	
1,3,	6,30,	1,8,	1,9,	
0,0,	1,10,	6,29,	7,31,	
8,33,	0,0,	6,29,	0,0,	
7,32,	0,0,	0,0,	6,29,	
7,31,	1,11,	0,0,	1,12,	
2,27,	7,31,	1,13,	11,39,	
12,40,	1,13,	26,56,	0,0,	
0,0,	2,8,	2,9,	0,0,	
6,29,	0,0,	0,0,	6,29,	
0,0,	0,0,	7,31,	0,0,	
0,0,	7,31,	0,0,	0,0,	
2,11,	0,0,	2,12,	0,0,	
0,0,	0,0,	0,0,	0,0,	
0,0,	0,0,	1,14,	0,0,	
0,0,	1,15,	1,16,	1,17,	
0,0,	22,52,	1,18,	18,47,	
23,53,	1,19,	42,63,	1,20,	
1,21,	25,55,	14,42,	1,22,	
15,43,	1,23,	1,24,	16,44,	
1,25,	16,45,	17,46,	19,48,	
21,51,	2,14,	20,49,	1,26,	
2,15,	2,16,	2,17,	24,54,	
20,50,	2,18,	44,64,	45,65,	
2,19,	46,66,	2,20,	2,21,	
27,57,	48,67,	2,22,	49,68,	
2,23,	2,24,	50,69,	2,25,	
52,70,	53,72,	27,58,	54,73,	
52,71,	9,34,	2,26,	9,35,	
9,35,	9,35,	9,35,	9,35,	
9,35,	9,35,	9,35,	9,35,	
9,35,	10,36,	55,74,	10,37,	
10,37,	10,37,	10,37,	10,37,	
10,37,	10,37,	10,37,	10,37,	
10,37,	57,75,	58,76,	64,80,	
66,81,	67,82,	70,83,	71,84,	
72,85,	73,86,	74,87,	10,38,	
10,38,	38,61,	10,38,	38,61,	
75,88,	76,89,	38,62,	38,62,	
38,62,	38,62,	38,62,	38,62,	
38,62,	38,62,	38,62,	38,62,	
80,92,	81,93,	13,41,	13,41,	
13,41,	13,41,	13,41,	13,41,	
13,41,	13,41,	13,41,	13,41,	
82,94,	83,95,	84,96,	10,38,	
10,38,	86,97,	10,38,	13,41,	
13,41,	13,41,	13,41,	13,41,	
13,41,	13,41,	13,41,	13,41,	
13,41,	13,41,	13,41,	13,41,	
13,41,	13,41,	13,41,	13,41,	
13,41,	13,41,	13,41,	13,41,	
13,41,	13,41,	13,41,	13,41,	
13,41,	87,98,	88,99,	60,79,	
60,79,	13,41,	60,79,	13,41,	
13,41,	13,41,	13,41,	13,41,	
13,41,	13,41,	13,41,	13,41,	
13,41,	13,41,	13,41,	13,41,	
13,41,	13,41,	13,41,	13,41,	
13,41,	13,41,	13,41,	13,41,	
13,41,	13,41,	13,41,	13,41,	
13,41,	33,33,	89,100,	60,79,	
60,79,	92,101,	60,79,	93,102,	
95,103,	33,33,	33,0,	96,104,	
99,105,	100,106,	102,107,	106,108,	
107,109,	35,35,	35,35,	35,35,	
35,35,	35,35,	35,35,	35,35,	
35,35,	35,35,	35,35,	108,110,	
0,0,	0,0,	0,0,	0,0,	
0,0,	0,0,	33,33,	0,0,	
0,0,	35,59,	35,59,	33,33,	
35,59,	0,0,	0,0,	33,33,	
0,0,	0,0,	0,0,	0,0,	
33,33,	0,0,	0,0,	0,0,	
0,0,	36,60,	36,60,	36,60,	
36,60,	36,60,	36,60,	36,60,	
36,60,	36,60,	36,60,	0,0,	
0,0,	33,33,	0,0,	0,0,	
33,33,	35,59,	35,59,	0,0,	
35,59,	36,38,	36,38,	59,77,	
36,38,	59,77,	0,0,	0,0,	
59,78,	59,78,	59,78,	59,78,	
59,78,	59,78,	59,78,	59,78,	
59,78,	59,78,	61,62,	61,62,	
61,62,	61,62,	61,62,	61,62,	
61,62,	61,62,	61,62,	61,62,	
0,0,	0,0,	0,0,	0,0,	
0,0,	36,38,	36,38,	0,0,	
36,38,	77,78,	77,78,	77,78,	
77,78,	77,78,	77,78,	77,78,	
77,78,	77,78,	77,78,	79,90,	
0,0,	79,90,	0,0,	0,0,	
79,91,	79,91,	79,91,	79,91,	
79,91,	79,91,	79,91,	79,91,	
79,91,	79,91,	90,91,	90,91,	
90,91,	90,91,	90,91,	90,91,	
90,91,	90,91,	90,91,	90,91,	
0,0};
struct yysvf yysvec[] = {
0,	0,	0,
yycrank+-1,	0,		yyvstop+1,
yycrank+-28,	yysvec+1,	yyvstop+3,
yycrank+0,	0,		yyvstop+5,
yycrank+4,	0,		yyvstop+7,
yycrank+0,	0,		yyvstop+10,
yycrank+-11,	0,		yyvstop+12,
yycrank+-17,	0,		yyvstop+14,
yycrank+7,	0,		yyvstop+16,
yycrank+107,	0,		yyvstop+18,
yycrank+119,	0,		yyvstop+20,
yycrank+6,	0,		yyvstop+23,
yycrank+7,	0,		yyvstop+25,
yycrank+158,	0,		yyvstop+27,
yycrank+4,	yysvec+13,	yyvstop+30,
yycrank+5,	yysvec+13,	yyvstop+33,
yycrank+11,	yysvec+13,	yyvstop+36,
yycrank+5,	yysvec+13,	yyvstop+39,
yycrank+5,	yysvec+13,	yyvstop+42,
yycrank+12,	yysvec+13,	yyvstop+45,
yycrank+21,	yysvec+13,	yyvstop+48,
yycrank+10,	yysvec+13,	yyvstop+51,
yycrank+4,	yysvec+13,	yyvstop+54,
yycrank+4,	yysvec+13,	yyvstop+57,
yycrank+21,	yysvec+13,	yyvstop+60,
yycrank+9,	yysvec+13,	yyvstop+63,
yycrank+9,	0,		yyvstop+66,
yycrank+40,	0,		yyvstop+68,
yycrank+0,	yysvec+4,	yyvstop+70,
yycrank+0,	yysvec+6,	0,	
yycrank+0,	0,		yyvstop+72,
yycrank+0,	yysvec+7,	0,	
yycrank+0,	0,		yyvstop+74,
yycrank+-280,	0,		yyvstop+76,
yycrank+0,	0,		yyvstop+78,
yycrank+249,	0,		yyvstop+80,
yycrank+285,	0,		yyvstop+82,
yycrank+0,	yysvec+10,	yyvstop+84,
yycrank+146,	0,		0,	
yycrank+0,	0,		yyvstop+86,
yycrank+0,	0,		yyvstop+88,
yycrank+0,	yysvec+13,	yyvstop+90,
yycrank+10,	yysvec+13,	yyvstop+92,
yycrank+0,	yysvec+13,	yyvstop+94,
yycrank+19,	yysvec+13,	yyvstop+97,
yycrank+35,	yysvec+13,	yyvstop+99,
yycrank+27,	yysvec+13,	yyvstop+101,
yycrank+0,	yysvec+13,	yyvstop+103,
yycrank+42,	yysvec+13,	yyvstop+106,
yycrank+35,	yysvec+13,	yyvstop+108,
yycrank+30,	yysvec+13,	yyvstop+110,
yycrank+0,	yysvec+13,	yyvstop+112,
yycrank+36,	yysvec+13,	yyvstop+115,
yycrank+48,	yysvec+13,	yyvstop+117,
yycrank+35,	yysvec+13,	yyvstop+119,
yycrank+61,	yysvec+13,	yyvstop+121,
yycrank+0,	0,		yyvstop+123,
yycrank+76,	0,		0,	
yycrank+67,	0,		0,	
yycrank+312,	0,		0,	
yycrank+183,	yysvec+36,	yyvstop+125,
yycrank+322,	0,		0,	
yycrank+0,	yysvec+61,	yyvstop+128,
yycrank+0,	yysvec+13,	yyvstop+130,
yycrank+78,	yysvec+13,	yyvstop+133,
yycrank+0,	yysvec+13,	yyvstop+135,
yycrank+81,	yysvec+13,	yyvstop+138,
yycrank+84,	yysvec+13,	yyvstop+140,
yycrank+0,	yysvec+13,	yyvstop+142,
yycrank+0,	yysvec+13,	yyvstop+145,
yycrank+81,	yysvec+13,	yyvstop+148,
yycrank+66,	yysvec+13,	yyvstop+150,
yycrank+74,	yysvec+13,	yyvstop+152,
yycrank+80,	yysvec+13,	yyvstop+154,
yycrank+78,	yysvec+13,	yyvstop+156,
yycrank+94,	0,		0,	
yycrank+93,	0,		0,	
yycrank+341,	0,		0,	
yycrank+0,	yysvec+77,	yyvstop+158,
yycrank+356,	0,		0,	
yycrank+99,	yysvec+13,	yyvstop+160,
yycrank+89,	yysvec+13,	yyvstop+163,
yycrank+108,	yysvec+13,	yyvstop+165,
yycrank+120,	yysvec+13,	yyvstop+167,
yycrank+104,	yysvec+13,	yyvstop+169,
yycrank+0,	yysvec+13,	yyvstop+171,
yycrank+113,	yysvec+13,	yyvstop+174,
yycrank+148,	yysvec+13,	yyvstop+176,
yycrank+133,	0,		0,	
yycrank+181,	0,		0,	
yycrank+366,	0,		0,	
yycrank+0,	yysvec+90,	yyvstop+178,
yycrank+183,	yysvec+13,	yyvstop+181,
yycrank+182,	yysvec+13,	yyvstop+183,
yycrank+0,	yysvec+13,	yyvstop+185,
yycrank+172,	yysvec+13,	yyvstop+189,
yycrank+181,	yysvec+13,	yyvstop+191,
yycrank+0,	yysvec+13,	yyvstop+193,
yycrank+0,	yysvec+13,	yyvstop+196,
yycrank+189,	0,		0,	
yycrank+195,	0,		0,	
yycrank+0,	yysvec+13,	yyvstop+199,
yycrank+183,	yysvec+13,	yyvstop+202,
yycrank+0,	yysvec+13,	yyvstop+204,
yycrank+0,	yysvec+13,	yyvstop+207,
yycrank+0,	0,		yyvstop+210,
yycrank+178,	0,		0,	
yycrank+186,	yysvec+13,	yyvstop+212,
yycrank+204,	0,		0,	
yycrank+0,	yysvec+13,	yyvstop+214,
yycrank+0,	0,		yyvstop+217,
0,	0,	0};
struct yywork *yytop = yycrank+423;
struct yysvf *yybgin = yysvec+1;
char yymatch[] = {
00  ,01  ,01  ,01  ,01  ,01  ,01  ,01  ,
01  ,011 ,012 ,01  ,01  ,01  ,01  ,01  ,
01  ,01  ,01  ,01  ,01  ,01  ,01  ,01  ,
01  ,01  ,01  ,01  ,01  ,01  ,01  ,01  ,
011 ,01  ,'"' ,01  ,01  ,01  ,01  ,047 ,
01  ,01  ,01  ,'+' ,01  ,'+' ,01  ,01  ,
'0' ,'0' ,'0' ,'0' ,'0' ,'0' ,'0' ,'0' ,
'0' ,'0' ,01  ,01  ,01  ,01  ,01  ,01  ,
01  ,'A' ,'A' ,'A' ,'D' ,'D' ,'A' ,'D' ,
'A' ,'A' ,'A' ,'A' ,'A' ,'A' ,'A' ,'A' ,
'A' ,'A' ,'A' ,'A' ,'A' ,'A' ,'A' ,'A' ,
'A' ,'A' ,'A' ,01  ,01  ,01  ,01  ,'A' ,
01  ,'A' ,'A' ,'A' ,'D' ,'D' ,'A' ,'D' ,
'A' ,'A' ,'A' ,'A' ,'A' ,'A' ,'A' ,'A' ,
'A' ,'A' ,'A' ,'A' ,'A' ,'A' ,'A' ,'A' ,
'A' ,'A' ,'A' ,01  ,01  ,01  ,01  ,01  ,
0};
char yyextra[] = {
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0};
#ifndef lint
static	char ncform_sccsid[] = "@(#)ncform 1.6 88/02/08 SMI"; /* from S5R2 1.2 */
#endif

int yylineno =1;
# define YYU(x) x
# define NLSTATE yyprevious=YYNEWLINE
char yytext[YYLMAX];
struct yysvf *yylstate [YYLMAX], **yylsp, **yyolsp;
char yysbuf[YYLMAX];
char *yysptr = yysbuf;
int *yyfnd;
extern struct yysvf *yyestate;
int yyprevious = YYNEWLINE;
yylook(){
	register struct yysvf *yystate, **lsp;
	register struct yywork *yyt;
	struct yysvf *yyz;
	int yych, yyfirst;
	struct yywork *yyr;
# ifdef LEXDEBUG
	int debug;
# endif
	char *yylastch;
	/* start off machines */
# ifdef LEXDEBUG
	debug = 0;
# endif
	yyfirst=1;
	if (!yymorfg)
		yylastch = yytext;
	else {
		yymorfg=0;
		yylastch = yytext+yyleng;
		}
	for(;;){
		lsp = yylstate;
		yyestate = yystate = yybgin;
		if (yyprevious==YYNEWLINE) yystate++;
		for (;;){
# ifdef LEXDEBUG
			if(debug)fprintf(yyout,"state %d\n",yystate-yysvec-1);
# endif
			yyt = yystate->yystoff;
			if(yyt == yycrank && !yyfirst){  /* may not be any transitions */
				yyz = yystate->yyother;
				if(yyz == 0)break;
				if(yyz->yystoff == yycrank)break;
				}
			*yylastch++ = yych = input();
			yyfirst=0;
		tryagain:
# ifdef LEXDEBUG
			if(debug){
				fprintf(yyout,"char ");
				allprint(yych);
				putchar('\n');
				}
# endif
			yyr = yyt;
			if ( (int)yyt > (int)yycrank){
				yyt = yyr + yych;
				if (yyt <= yytop && yyt->verify+yysvec == yystate){
					if(yyt->advance+yysvec == YYLERR)	/* error transitions */
						{unput(*--yylastch);break;}
					*lsp++ = yystate = yyt->advance+yysvec;
					goto contin;
					}
				}
# ifdef YYOPTIM
			else if((int)yyt < (int)yycrank) {		/* r < yycrank */
				yyt = yyr = yycrank+(yycrank-yyt);
# ifdef LEXDEBUG
				if(debug)fprintf(yyout,"compressed state\n");
# endif
				yyt = yyt + yych;
				if(yyt <= yytop && yyt->verify+yysvec == yystate){
					if(yyt->advance+yysvec == YYLERR)	/* error transitions */
						{unput(*--yylastch);break;}
					*lsp++ = yystate = yyt->advance+yysvec;
					goto contin;
					}
				yyt = yyr + YYU(yymatch[yych]);
# ifdef LEXDEBUG
				if(debug){
					fprintf(yyout,"try fall back character ");
					allprint(YYU(yymatch[yych]));
					putchar('\n');
					}
# endif
				if(yyt <= yytop && yyt->verify+yysvec == yystate){
					if(yyt->advance+yysvec == YYLERR)	/* error transition */
						{unput(*--yylastch);break;}
					*lsp++ = yystate = yyt->advance+yysvec;
					goto contin;
					}
				}
			if ((yystate = yystate->yyother) && (yyt= yystate->yystoff) != yycrank){
# ifdef LEXDEBUG
				if(debug)fprintf(yyout,"fall back to state %d\n",yystate-yysvec-1);
# endif
				goto tryagain;
				}
# endif
			else
				{unput(*--yylastch);break;}
		contin:
# ifdef LEXDEBUG
			if(debug){
				fprintf(yyout,"state %d char ",yystate-yysvec-1);
				allprint(yych);
				putchar('\n');
				}
# endif
			;
			}
# ifdef LEXDEBUG
		if(debug){
			fprintf(yyout,"stopped at %d with ",*(lsp-1)-yysvec-1);
			allprint(yych);
			putchar('\n');
			}
# endif
		while (lsp-- > yylstate){
			*yylastch-- = 0;
			if (*lsp != 0 && (yyfnd= (*lsp)->yystops) && *yyfnd > 0){
				yyolsp = lsp;
				if(yyextra[*yyfnd]){		/* must backup */
					while(yyback((*lsp)->yystops,-*yyfnd) != 1 && lsp > yylstate){
						lsp--;
						unput(*yylastch--);
						}
					}
				yyprevious = YYU(*yylastch);
				yylsp = lsp;
				yyleng = yylastch-yytext+1;
				yytext[yyleng] = 0;
# ifdef LEXDEBUG
				if(debug){
					fprintf(yyout,"\nmatch ");
					sprint(yytext);
					fprintf(yyout," action %d\n",*yyfnd);
					}
# endif
				return(*yyfnd++);
				}
			unput(*yylastch);
			}
		if (yytext[0] == 0  /* && feof(yyin) */)
			{
			yysptr=yysbuf;
			return(0);
			}
		yyprevious = yytext[0] = input();
		if (yyprevious>0)
			output(yyprevious);
		yylastch=yytext;
# ifdef LEXDEBUG
		if(debug)putchar('\n');
# endif
		}
	}
yyback(p, m)
	int *p;
{
if (p==0) return(0);
while (*p)
	{
	if (*p++ == m)
		return(1);
	}
return(0);
}
	/* the following are only used in the lex library */
yyinput(){
	return(input());
	}
yyoutput(c)
  int c; {
	output(c);
	}
yyunput(c)
   int c; {
	unput(c);
	}
