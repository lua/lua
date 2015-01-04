char *rcs_lex = "$Id: lex.c,v 2.39 1996/11/08 19:08:30 roberto Exp $";


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

#define next() (current = input())
#define save(x) (yytext[tokensize++] = (x))
#define save_and_next()  (save(current), next())


static int current;  /* look ahead character */
static Input input;  /* input function */


void lua_setinput (Input fn)
{
  current = '\n';
  lua_linenumber = 0;
  input = fn;
}

void luaI_syntaxerror (char *s)
{
  char msg[256];
  char *token = luaI_buffer(1);
  if (token[0] == 0)
    token = "<eof>";
  sprintf (msg,"%s;\n> last token read: \"%s\" at line %d in file %s",
           s, token, lua_linenumber, lua_parsedfile);
  lua_error (msg);
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

static int inclinenumber (int pragma_allowed)
{
  ++lua_linenumber;
  if (pragma_allowed && current == '$') {  /* is a pragma? */
    char *buff = luaI_buffer(MINBUFF+1);
    int i = 0;
    next();  /* skip $ */
    while (isalnum(current)) {
      if (i >= MINBUFF) luaI_syntaxerror("pragma too long");
      buff[i++] = current;
      next();
    }
    buff[i] = 0;
    if (strcmp(buff, "debug") == 0)
      lua_debug = 1;
    else if (strcmp(buff, "nodebug") == 0)
      lua_debug = 0;
    else luaI_syntaxerror("invalid pragma");
  }
  return lua_linenumber;
}

static int read_long_string (char *yytext, int buffsize)
{
  int cont = 0;
  int tokensize = 2;  /* '[[' already stored */
  while (1)
  {
    if (buffsize-tokensize <= 2) /* may read more than 1 char in one cicle */
      yytext = luaI_buffer(buffsize *= 2);
    switch (current)
    {
      case 0:
        save(0);
        return WRONGTOKEN;
      case '[':
        save_and_next();
        if (current == '[')
        {
          cont++;
          save_and_next();
        }
        continue;
      case ']':
        save_and_next();
        if (current == ']')
        {
          if (cont == 0) goto endloop;
          cont--;
          save_and_next();
        }
        continue;
      case '\n':
        save_and_next();
        inclinenumber(0);
        continue;
      default:
        save_and_next();
    }
  } endloop:
  save_and_next();  /* pass the second ']' */
  yytext[tokensize-2] = 0;  /* erases ']]' */
  luaY_lval.vWord = luaI_findconstantbyname(yytext+2);
  yytext[tokensize-2] = ']';  /* restores ']]' */
  save(0);
  return STRING;
}

int luaY_lex (void)
{
  static int linelasttoken = 0;
  double a;
  int buffsize = MINBUFF;
  char *yytext = luaI_buffer(buffsize);
  yytext[1] = yytext[2] = yytext[3] = 0;
  if (lua_debug)
    luaI_codedebugline(linelasttoken);
  linelasttoken = lua_linenumber;
  while (1)
  {
    int tokensize = 0;
    switch (current)
    {
      case '\n':
        next();
        linelasttoken = inclinenumber(1);
        continue;

      case ' ': case '\t': case '\r':  /* CR: to avoid problems with DOS */
        next();
        continue;

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
          return read_long_string(yytext, buffsize);
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
        save_and_next();
        while (current != del)
        {
          if (buffsize-tokensize <= 2) /* may read more than 1 char in one cicle */
            yytext = luaI_buffer(buffsize *= 2);
          switch (current)
          {
            case 0:
            case '\n':
              save(0);
              return WRONGTOKEN;
            case '\\':
              next();  /* do not save the '\' */
              switch (current)
              {
                case 'n': save('\n'); next(); break;
                case 't': save('\t'); next(); break;
                case 'r': save('\r'); next(); break;
                case '\n': save_and_next(); inclinenumber(0); break;
                default : save_and_next(); break;
              }
              break;
            default:
              save_and_next();
          }
        }
        next();  /* skip delimiter */
        save(0);
        luaY_lval.vWord = luaI_findconstantbyname(yytext+1);
        tokensize--;
        save(del); save(0);  /* restore delimiter */
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
        save(0);
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
          if (current == '.')
          {
            save_and_next();
            return DOTS;   /* ... */
          }
          else return CONC;   /* .. */
        }
        else if (!isdigit(current)) return '.';
        /* current is a digit: goes through to number */
	a=0.0;
        goto fraction;

      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
	a=0.0;
        do {
          a=10.0*a+(current-'0');
          save_and_next();
        } while (isdigit(current));
        if (current == '.') {
          save_and_next();
          if (current == '.')
            luaI_syntaxerror(
              "ambiguous syntax (decimal point x string concatenation)");
        }
      fraction:
	{ double da=0.1;
	  while (isdigit(current))
	  {
            a+=(current-'0')*da;
            da/=10.0;
            save_and_next();
          }
          if (current == 'e' || current == 'E')
          {
	    int e=0;
	    int neg;
	    double ea;
            save_and_next();
	    neg=(current=='-');
            if (current == '+' || current == '-') save_and_next();
            if (!isdigit(current)) { save(0); return WRONGTOKEN; }
            do {
              e=10.0*e+(current-'0');
              save_and_next();
            } while (isdigit(current));
	    for (ea=neg?0.1:10.0; e>0; e>>=1)
	    {
	      if (e & 1) a*=ea;
	      ea*=ea;
	    }
          }
          luaY_lval.vFloat = a;
          save(0);
          return NUMBER;
        }

      default: 		/* also end of program (0) */
      {
        save_and_next();
        return yytext[0];
      }
    }
  }
}

