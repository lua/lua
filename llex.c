/*
** $Id: llex.c,v 1.2 1997/09/26 15:02:26 roberto Exp roberto $
** Lexical Analizer
** See Copyright Notice in lua.h
*/


#include <ctype.h>
#include <string.h>

#include "llex.h"
#include "lmem.h"
#include "lobject.h"
#include "lparser.h"
#include "lstring.h"
#include "lstx.h"
#include "luadebug.h"
#include "lzio.h"


static int current;  /* look ahead character */
static ZIO *lex_z;


int luaX_linenumber;
int lua_debug=0;


#define next() (current = zgetc(lex_z))



static void addReserved (void)
{
  static struct {
    char *name;
    int token;
  } reserved [] = {
      {"and", AND}, {"do", DO}, {"else", ELSE}, {"elseif", ELSEIF},
      {"end", END}, {"function", FUNCTION}, {"if", IF}, {"local", LOCAL},
      {"nil", NIL}, {"not", NOT}, {"or", OR}, {"repeat", REPEAT},
      {"return", RETURN}, {"then", THEN}, {"until", UNTIL}, {"while", WHILE}
    };
  static int firsttime = 1;
  if (firsttime) {
    int i;
    firsttime = 0;
    for (i=0; i<(sizeof(reserved)/sizeof(reserved[0])); i++) {
      TaggedString *ts = luaS_new(reserved[i].name);
      ts->head.marked = reserved[i].token;  /* reserved word  (always > 255) */
    }
  }
}



#define MAX_IFS	5

/* "ifstate" keeps the state of each nested $if the lexical is dealing with. */

static struct {
  int elsepart;  /* true if its in the $else part */
  int condition;  /* true if $if condition is true */
  int skip;  /* true if part must be skiped */
} ifstate[MAX_IFS];

static int iflevel;  /* level of nested $if's */


static struct textbuff {
  char *text;
  int tokensize;
  int buffsize;
} textbuff;


static void firstline (void)
{
  int c = zgetc(lex_z);
  if (c == '#')
    while((c=zgetc(lex_z)) != '\n' && c != EOZ) /* skip first line */;
  zungetc(lex_z);
}


void luaX_setinput (ZIO *z)
{
  addReserved();
  current = '\n';
  luaX_linenumber = 0;
  iflevel = 0;
  ifstate[0].skip = 0;
  ifstate[0].elsepart = 1;  /* to avoid a free $else */
  lex_z = z;
  firstline();
  textbuff.buffsize = 20;
  textbuff.text = luaM_buffer(textbuff.buffsize);
}



/*
** =======================================================
** PRAGMAS
** =======================================================
*/

#define PRAGMASIZE	20

static void skipspace (void)
{
  while (current == ' ' || current == '\t') next();
}


static int checkcond (char *buff)
{
  static char *opts[] = {"nil", "1"};
  int i = luaO_findstring(buff, opts);
  if (i >= 0) return i;
  else if (isalpha((unsigned char)buff[0]) || buff[0] == '_')
    return luaS_globaldefined(buff);
  else {
    luaY_syntaxerror("invalid $if condition", buff);
    return 0;  /* to avoid warnings */
  }
}


static void readname (char *buff)
{
  int i = 0;
  skipspace();
  while (isalnum(current) || current == '_') {
    if (i >= PRAGMASIZE) {
      buff[PRAGMASIZE] = 0;
      luaY_syntaxerror("pragma too long", buff);
    }
    buff[i++] = current;
    next();
  }
  buff[i] = 0;
}


static void inclinenumber (void);


static void ifskip (void)
{
  while (ifstate[iflevel].skip) {
    if (current == '\n')
      inclinenumber();
    else if (current == EOZ)
      luaY_syntaxerror("input ends inside a $if", "");
    else next();
  }
}


static void inclinenumber (void)
{
  static char *pragmas [] =
    {"debug", "nodebug", "endinput", "end", "ifnot", "if", "else", NULL};
  next();  /* skip '\n' */
  ++luaX_linenumber;
  if (current == '$') {  /* is a pragma? */
    char buff[PRAGMASIZE+1];
    int ifnot = 0;
    int skip = ifstate[iflevel].skip;
    next();  /* skip $ */
    readname(buff);
    switch (luaO_findstring(buff, pragmas)) {
      case 0:  /* debug */
        if (!skip) lua_debug = 1;
        break;
      case 1:  /* nodebug */
        if (!skip) lua_debug = 0;
        break;
      case 2:  /* endinput */
        if (!skip) {
          current = EOZ;
          iflevel = 0;  /* to allow $endinput inside a $if */
        }
        break;
      case 3:  /* end */
        if (iflevel-- == 0)
          luaY_syntaxerror("unmatched $end", "$end");
        break;
      case 4:  /* ifnot */
        ifnot = 1;
        /* go through */
      case 5:  /* if */
        if (iflevel == MAX_IFS-1)
          luaY_syntaxerror("too many nested `$ifs'", "$if");
        readname(buff);
        iflevel++;
        ifstate[iflevel].elsepart = 0;
        ifstate[iflevel].condition = checkcond(buff) ? !ifnot : ifnot;
        ifstate[iflevel].skip = skip || !ifstate[iflevel].condition;
        break;
      case 6:  /* else */
        if (ifstate[iflevel].elsepart)
          luaY_syntaxerror("unmatched $else", "$else");
        ifstate[iflevel].elsepart = 1;
        ifstate[iflevel].skip =
                    ifstate[iflevel-1].skip || ifstate[iflevel].condition;
        break;
      default:
        luaY_syntaxerror("invalid pragma", buff);
    }
    skipspace();
    if (current == '\n')  /* pragma must end with a '\n' ... */
      inclinenumber();
    else if (current != EOZ)  /* or eof */
      luaY_syntaxerror("invalid pragma format", buff);
    ifskip();
  }
}


/*
** =======================================================
** LEXICAL ANALIZER
** =======================================================
*/



static void save (int c)
{
  if (textbuff.tokensize >= textbuff.buffsize)
    textbuff.text = luaM_buffer(textbuff.buffsize *= 2);
  textbuff.text[textbuff.tokensize++] = c;
}


char *luaX_lasttoken (void)
{
  save(0);
  return textbuff.text;
}


#define save_and_next()  (save(current), next())


static int read_long_string (void)
{
  int cont = 0;
  while (1) {
    switch (current) {
      case EOZ:
        save(0);
        return WRONGTOKEN;
      case '[':
        save_and_next();
        if (current == '[') {
          cont++;
          save_and_next();
        }
        continue;
      case ']':
        save_and_next();
        if (current == ']') {
          if (cont == 0) goto endloop;
          cont--;
          save_and_next();
        }
        continue;
      case '\n':
        save('\n');
        inclinenumber();
        continue;
      default:
        save_and_next();
    }
  } endloop:
  save_and_next();  /* pass the second ']' */
  textbuff.text[textbuff.tokensize-2] = 0;  /* erases ']]' */
  luaY_lval.pTStr = luaS_new(textbuff.text+2);
  textbuff.text[textbuff.tokensize-2] = ']';  /* restores ']]' */
  return STRING;
}


int luaY_lex (void)
{
  static int linelasttoken = 0;
  double a;
  textbuff.tokensize = 0;
  if (lua_debug)
    luaY_codedebugline(linelasttoken);
  linelasttoken = luaX_linenumber;
  while (1) {
    switch (current) {
      case '\n':
        inclinenumber();
        linelasttoken = luaX_linenumber;
        continue;

      case ' ': case '\t': case '\r':  /* CR: to avoid problems with DOS */
        next();
        continue;

      case '-':
        save_and_next();
        if (current != '-') return '-';
        do { next(); } while (current != '\n' && current != EOZ);
        textbuff.tokensize = 0;
        continue;

      case '[':
        save_and_next();
        if (current != '[') return '[';
        else {
          save_and_next();  /* pass the second '[' */
          return read_long_string();
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
      case '\'': {
        int del = current;
        save_and_next();
        while (current != del) {
          switch (current) {
            case EOZ:
            case '\n':
              save(0);
              return WRONGTOKEN;
            case '\\':
              next();  /* do not save the '\' */
              switch (current) {
                case 'n': save('\n'); next(); break;
                case 't': save('\t'); next(); break;
                case 'r': save('\r'); next(); break;
                case '\n': save('\n'); inclinenumber(); break;
                default : save_and_next(); break;
              }
              break;
            default:
              save_and_next();
          }
        }
        next();  /* skip delimiter */
        save(0);
        luaY_lval.pTStr = luaS_new(textbuff.text+1);
        textbuff.text[textbuff.tokensize-1] = del;  /* restore delimiter */
        return STRING;
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
          if (current == '.') {
            save(0);
            luaY_error(
              "ambiguous syntax (decimal point x string concatenation)");
          }
        }
      fraction:
	{ double da=0.1;
	  while (isdigit(current))
	  {
            a+=(current-'0')*da;
            da/=10.0;
            save_and_next();
          }
          if (toupper(current) == 'E') {
	    int e=0;
	    int neg;
	    double ea;
            save_and_next();
	    neg=(current=='-');
            if (current == '+' || current == '-') save_and_next();
            if (!isdigit(current)) {
              save(0); return WRONGTOKEN; }
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
          luaY_lval.vReal = a;
          return NUMBER;
        }

      case EOZ:
        save(0);
        if (iflevel > 0)
          luaY_error("missing $endif");
        return 0;

      default:
        if (current != '_' && !isalpha(current)) {
          save_and_next();
          return textbuff.text[0];
        }
        else {  /* identifier or reserved word */
          TaggedString *ts;
          do {
            save_and_next();
          } while (isalnum(current) || current == '_');
          save(0);
          ts = luaS_new(textbuff.text);
          if (ts->head.marked > 255)
            return ts->head.marked;  /* reserved word */
          luaY_lval.pTStr = ts;
          return NAME;
        }
    }
  }
}

