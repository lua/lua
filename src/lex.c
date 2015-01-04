char *rcs_lex = "$Id: lex.c,v 2.1 1994/04/15 19:00:28 celes Exp $";
/*$Log: lex.c,v $
 * Revision 2.1  1994/04/15  19:00:28  celes
 * Retirar chamada da funcao lua_findsymbol associada a cada
 * token NAME. A decisao de chamar lua_findsymbol ou lua_findconstant
 * fica a cargo do modulo "lua.stx".
 *
 * Revision 1.3  1993/12/28  16:42:29  roberto
 * "include"s de string.h e stdlib.h para evitar warnings
 *
 * Revision 1.2  1993/12/22  21:39:15  celes
 * Tratamento do token $debug e $nodebug
 *
 * Revision 1.1  1993/12/22  21:15:16  roberto
 * Initial revision
 **/

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "opcode.h"
#include "hash.h"
#include "inout.h"
#include "table.h"
#include "y.tab.h"

#define next() { current = input(); }
#define save(x) { *yytextLast++ = (x); }
#define save_and_next()  { save(current); next(); }

static int current;
static char yytext[256];
static char *yytextLast;

static Input input;

void lua_setinput (Input fn)
{
  current = ' ';
  input = fn;
}

char *lua_lasttext (void)
{
  *yytextLast = 0;
  return yytext;
}


static struct 
  {
    char *name;
    int token;
  } reserved [] = {
      {"and", AND},
      {"do", DO},
      {"else", ELSE},
      {"elseif", ELSEIF},
      {"end", END},
      {"function", FUNCTION},
      {"if", IF},
      {"local", LOCAL},
      {"nil", NIL},
      {"not", NOT},
      {"or", OR},
      {"repeat", REPEAT},
      {"return", RETURN},
      {"then", THEN},
      {"until", UNTIL},
      {"while", WHILE} };

#define RESERVEDSIZE (sizeof(reserved)/sizeof(reserved[0]))


int findReserved (char *name)
{
  int l = 0;
  int h = RESERVEDSIZE - 1;
  while (l <= h)
  {
    int m = (l+h)/2;
    int comp = strcmp(name, reserved[m].name);
    if (comp < 0)
      h = m-1;
    else if (comp == 0)
      return reserved[m].token;
    else
      l = m+1;
  }
  return 0;
}


int yylex ()
{
  while (1)
  {
    yytextLast = yytext;
    switch (current)
    {
      case '\n': lua_linenumber++;
      case ' ':
      case '\t':
        next();
        continue;

      case '$':
	next();
	while (isalnum(current) || current == '_')
          save_and_next();
        *yytextLast = 0;
	if (strcmp(yytext, "debug") == 0)
	{
	  yylval.vInt = 1;
	  return DEBUG;
        }
	else if (strcmp(yytext, "nodebug") == 0)
	{
	  yylval.vInt = 0;
	  return DEBUG;
        }
	return WRONGTOKEN;
  
      case '-':
        save_and_next();
        if (current != '-') return '-';
        do { next(); } while (current != '\n' && current != 0);
        continue;
  
      case '<':
        save_and_next();
        if (current != '=') return '<';
        else { save_and_next(); return LE; }
  
      case '>':
        save_and_next();
        if (current != '=') return '>';
        else { save_and_next(); return GE; }
  
      case '~':
        save_and_next();
        if (current != '=') return '~';
        else { save_and_next(); return NE; }

      case '"':
      case '\'':
      {
        int del = current;
        next();  /* skip the delimiter */
        while (current != del) 
        {
          switch (current)
          {
            case 0: 
            case '\n': 
              return WRONGTOKEN;
            case '\\':
              next();  /* do not save the '\' */
              switch (current)
              {
                case 'n': save('\n'); next(); break;
                case 't': save('\t'); next(); break;
                case 'r': save('\r'); next(); break;
                default : save('\\'); break;
              }
              break;
            default: 
              save_and_next();
          }
        }
        next();  /* skip the delimiter */
        *yytextLast = 0;
        yylval.vWord = lua_findconstant (yytext);
        return STRING;
      }

      case 'a': case 'b': case 'c': case 'd': case 'e':
      case 'f': case 'g': case 'h': case 'i': case 'j':
      case 'k': case 'l': case 'm': case 'n': case 'o':
      case 'p': case 'q': case 'r': case 's': case 't':
      case 'u': case 'v': case 'w': case 'x': case 'y':
      case 'z':
      case 'A': case 'B': case 'C': case 'D': case 'E':
      case 'F': case 'G': case 'H': case 'I': case 'J':
      case 'K': case 'L': case 'M': case 'N': case 'O':
      case 'P': case 'Q': case 'R': case 'S': case 'T':
      case 'U': case 'V': case 'W': case 'X': case 'Y':
      case 'Z':
      case '_':
      {
        int res;
        do { save_and_next(); } while (isalnum(current) || current == '_');
        *yytextLast = 0;
        res = findReserved(yytext);
        if (res) return res;
        yylval.pChar = yytext;
        return NAME;
      }
   
      case '.':
        save_and_next();
        if (current == '.') 
        { 
          save_and_next(); 
          return CONC;
        }
        else if (!isdigit(current)) return '.';
        /* current is a digit: goes through to number */
        goto fraction;

      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
      
        do { save_and_next(); } while (isdigit(current));
        if (current == '.') save_and_next();
fraction: while (isdigit(current)) save_and_next();
        if (current == 'e' || current == 'E')
        {
          save_and_next();
          if (current == '+' || current == '-') save_and_next();
          if (!isdigit(current)) return WRONGTOKEN;
          do { save_and_next(); } while (isdigit(current));
        }
        *yytextLast = 0;
        yylval.vFloat = atof(yytext);
        return NUMBER;

      default: 		/* also end of file */
      {
        save_and_next();
        return *yytext;      
      }
    }
  }
}
        
