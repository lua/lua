char *rcs_lex = "$Id: lex.c,v 2.45 1997/04/01 21:23:20 roberto Exp roberto $";


#include <ctype.h>
#include <string.h>

#include "auxlib.h"
#include "luamem.h"
#include "tree.h"
#include "table.h"
#include "lex.h"
#include "inout.h"
#include "luadebug.h"
#include "parser.h"

#define MINBUFF 250

#define next() (current = input())
#define save(x) (yytext[tokensize++] = (x))
#define save_and_next()  (save(current), next())


static int current;  /* look ahead character */
static Input input;  /* input function */
static int iflevel;  /* level of nested $if's */


void lua_setinput (Input fn)
{
  current = '\n';
  lua_linenumber = 0;
  iflevel = 0;
  input = fn;
}

static void luaI_auxsyntaxerror (char *s, char *token)
{
  if (token == NULL)
    luaL_verror("%s;\n> at line %d in file %s",
                 s, lua_linenumber, lua_parsedfile);
  if (token[0] == 0)
    token = "<eof>";
  luaL_verror("%s;\n> last token read: \"%s\" at line %d in file %s",
           s, token, lua_linenumber, lua_parsedfile);
}

void luaI_syntaxerror (char *s)
{
  luaI_auxsyntaxerror(s, luaI_buffer(1));
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


static void readname (char *buff)
{
  int i = 0;
  while (current == ' ') next();
  while (isalnum((unsigned char)current)) {
    if (i >= MINBUFF) luaI_syntaxerror("pragma too long");
    buff[i++] = current;
    next();
  }
  buff[i] = 0;
  if (!isalpha(buff[0]) || (current != 0 && !isspace(current)))
    luaI_auxsyntaxerror("invalid pragma format", NULL);
}

static int inclinenumber (void)
{
  static char *pragmas [] = {"debug", "nodebug", "endif", "ifnil", "if", NULL};
  int ifnil = 0;
  ++lua_linenumber;
  if (current == '$') {  /* is a pragma? */
    char buff[MINBUFF+1];
    next();  /* skip $ */
    readname(buff);
    switch (luaI_findstring(buff, pragmas)) {
      case 0:  /* debug */
        lua_debug = 1;
        break;
      case 1:  /* nodebug */
        lua_debug = 0;
        break;
      case 2:  /* endif */
        if (--iflevel < 0)
          luaI_auxsyntaxerror("too many $endif's", NULL);
        break;
      case 3:  /* ifnil */
        ifnil = 1;
        /* go through */
      case 4: {  /* if */
        int thisiflevel = iflevel++;
        readname(buff);
        if ((ifnil && luaI_globaldefined(buff)) ||
            (!ifnil && !luaI_globaldefined(buff))) {  /* skip the $if? */
          do {
            if (current == '\n') {
              next();
              inclinenumber();
            }
            else if (current == 0)
              luaI_auxsyntaxerror("input ends inside a $if", NULL);
            else next();
          } while (iflevel > thisiflevel);
        }
        break;
      }
      default: luaI_auxsyntaxerror("invalid pragma", buff);
    }
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
        inclinenumber();
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
        linelasttoken = inclinenumber();
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
                case '\n': save_and_next(); inclinenumber(); break;
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
        do {
          save_and_next();
        } while (isalnum((unsigned char)current) || current == '_');
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
        else if (!isdigit((unsigned char)current)) return '.';
        /* current is a digit: goes through to number */
	a=0.0;
        goto fraction;

      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
	a=0.0;
        do {
          a=10.0*a+(current-'0');
          save_and_next();
        } while (isdigit((unsigned char)current));
        if (current == '.') {
          save_and_next();
          if (current == '.')
            luaI_syntaxerror(
              "ambiguous syntax (decimal point x string concatenation)");
        }
      fraction:
	{ double da=0.1;
	  while (isdigit((unsigned char)current))
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
            if (!isdigit((unsigned char)current)) {
              save(0); return WRONGTOKEN; }
            do {
              e=10.0*e+(current-'0');
              save_and_next();
            } while (isdigit((unsigned char)current));
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

      case 0:
        save(0);
        if (iflevel > 0)
          luaI_syntaxerror("missing $endif");
        return 0;

      default:
        save_and_next();
        return yytext[0];
    }
  }
}

