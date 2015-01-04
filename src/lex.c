char *rcs_lex = "$Id: lex.c,v 2.32 1996/03/21 16:33:47 roberto Exp $";
 

#include <ctype.h>
#include <string.h>

#include "mem.h"
#include "tree.h"
#include "table.h"
#include "lex.h"
#include "inout.h"
#include "luadebug.h"
#include "parser.h"

#define MINBUFF 260

#define next() { current = input(); }
#define save(x) { *yytextLast++ = (x); }
#define save_and_next()  { save(current); next(); }

static int current;
static char *yytext = NULL;
static int textsize = 0;
static char *yytextLast;

static Input input;

void lua_setinput (Input fn)
{
  current = ' ';
  input = fn;
  if (yytext == NULL)
  {
    textsize = MINBUFF;
    yytext = newvector(textsize, char);
  }
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


void luaI_addReserved (void)
{
  int i;
  for (i=0; i<RESERVEDSIZE; i++)
  {
    TaggedString *ts = lua_createstring(reserved[i].name);
    ts->marked = reserved[i].token;  /* reserved word  (always > 255) */
  }
}


static void growtext (void)
{
  int size = yytextLast - yytext;
  textsize = growvector(&yytext, textsize, char, lexEM, MAX_WORD);
  yytextLast = yytext + size;
}


static int read_long_string (void)
{
  int cont = 0;
  int spaceleft = textsize - (yytextLast - yytext);
  while (1)
  {
    if (spaceleft <= 2)  /* may read more than 1 char in one cicle */
    {
      growtext();
      spaceleft = textsize - (yytextLast - yytext);
    }
    switch (current)
    {
      case EOF:
      case 0:
        return WRONGTOKEN;
      case '[':
        save_and_next(); spaceleft--;
        if (current == '[')
        {
          cont++;
          save_and_next(); spaceleft--;
        }
        continue; 
      case ']':
        save_and_next(); spaceleft--;
        if (current == ']')
        {
          if (cont == 0) return STRING;
          cont--;
          save_and_next(); spaceleft--;
        }
        continue; 
      case '\n':
        lua_linenumber++;  /* goes through */
      default:
        save_and_next(); spaceleft--;
    }
  }
}


int luaY_lex (void)
{
  double a;
  static int linelasttoken = 0;
  if (lua_debug)
    luaI_codedebugline(linelasttoken);
  linelasttoken = lua_linenumber;
  while (1)
  {
    yytextLast = yytext;
    switch (current)
    {
      case EOF:
      case 0:
       return 0;
      case '\n': linelasttoken = ++lua_linenumber;
      case ' ':
      case '\r':  /* CR: to avoid problems with DOS/Windows */
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
	  luaY_lval.vInt = 1;
	  return DEBUG;
        }
	else if (strcmp(yytext, "nodebug") == 0)
	{
	  luaY_lval.vInt = 0;
	  return DEBUG;
        }
	return WRONGTOKEN;

      case '-':
        save_and_next();
        if (current != '-') return '-';
        do { next(); } while (current != '\n' && current != 0);
        continue;

      case '[':
        save_and_next();
        if (current != '[') return '[';
        else
        {
          save_and_next();  /* pass the second '[' */
          if (read_long_string() == WRONGTOKEN)
            return WRONGTOKEN;
          save_and_next();  /* pass the second ']' */
          *(yytextLast-2) = 0;  /* erases ']]' */
          luaY_lval.vWord = luaI_findconstantbyname(yytext+2);
          return STRING;
        }

      case '=':
        save_and_next();
        if (current != '=') return '=';
        else { save_and_next(); return EQ; }

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
        int spaceleft = textsize - (yytextLast - yytext);
        next();  /* skip the delimiter */
        while (current != del)
        {
          if (spaceleft <= 2) /* may read more than 1 char in one cicle */
          {
            growtext();
            spaceleft = textsize - (yytextLast - yytext);
          }
          spaceleft--;
          switch (current)
          {
            case EOF:
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
                case '\n': lua_linenumber++;  /* goes through */
                default : save(current); next(); break;
              }
              break;
            default:
              save_and_next();
          }
        }
        next();  /* skip the delimiter */
        *yytextLast = 0;
        luaY_lval.vWord = luaI_findconstantbyname(yytext);
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
        TaggedString *ts;
        do { save_and_next(); } while (isalnum(current) || current == '_');
        *yytextLast = 0;
        ts = lua_createstring(yytext);
        if (ts->marked > 2)
          return ts->marked;  /* reserved word */
        luaY_lval.pTStr = ts;
        ts->marked = 2;  /* avoid GC */
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
	a=0.0;
        goto fraction;

      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
	a=0.0;
        do { a=10.0*a+(current-'0'); save_and_next(); } while (isdigit(current));
        if (current == '.') save_and_next();
fraction:
	{ double da=0.1;
	  while (isdigit(current))
	  {a+=(current-'0')*da; da/=10.0; save_and_next()};
          if (current == 'e' || current == 'E')
          {
	    int e=0;
	    int neg;
	    double ea;
            save_and_next();
	    neg=(current=='-');
            if (current == '+' || current == '-') save_and_next();
            if (!isdigit(current)) return WRONGTOKEN;
            do { e=10.0*e+(current-'0'); save_and_next(); } while (isdigit(current));
	    for (ea=neg?0.1:10.0; e>0; e>>=1) 
	    {
	      if (e & 1) a*=ea;
	      ea*=ea;
	    }
          }
          luaY_lval.vFloat = a;
          return NUMBER;
        }

      default: 		/* also end of file */
      {
        save_and_next();
        return yytext[0];
      }
    }
  }
}
