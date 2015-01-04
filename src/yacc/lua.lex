%{

char *rcs_lualex = "$Id: lua.lex,v 1.1 1993/12/17 18:53:41 celes Exp $";

#include <stdlib.h>
#include <string.h>

#include "opcode.h"
#include "hash.h"
#include "inout.h"
#include "table.h"
#include "y.tab.h"

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

%}


%%
[ \t]*					;
^"$debug"				{yylval.vInt = 1; return DEBUG;}
^"$nodebug"				{yylval.vInt = 0; return DEBUG;}
\n					lua_linenumber++;
"--".*					;
"local"					return LOCAL;
"if"					return IF;
"then"					return THEN;
"else"					return ELSE;
"elseif"				return ELSEIF;
"while"					return WHILE;
"do"					return DO;
"repeat"				return REPEAT;
"until"					return UNTIL;
"function"				{
                                         yylval.vWord = lua_nfile-1;
                                         return FUNCTION;
					}
"end"					return END;
"return" 				return RETURN;
"local" 				return LOCAL;
"nil"					return NIL;
"and"					return AND;
"or"					return OR;
"not"					return NOT;
"~="					return NE;
"<="					return LE;
">="					return GE;
".."					return CONC;
\"[^\"]*\" 			| 
\'[^\']*\'			      {
				       yylval.vWord = lua_findenclosedconstant (yytext);
				       return STRING;
				      }
[0-9]+("."[0-9]*)?	|
([0-9]+)?"."[0-9]+	|
[0-9]+("."[0-9]*)?[dDeEgG][+-]?[0-9]+ |
([0-9]+)?"."[0-9]+[dDeEgG][+-]?[0-9]+ {
				        yylval.vFloat = atof(yytext);
				        return NUMBER;
				       }
[a-zA-Z_][a-zA-Z0-9_]*  	       {
					yylval.vWord = lua_findsymbol (yytext);
					return NAME;
				       }
.					return  *yytext;

