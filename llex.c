/*
** $Id: llex.c,v 1.5 1997/11/07 15:09:49 roberto Exp roberto $
** Lexical Analizer
** See Copyright Notice in lua.h
*/


#include <ctype.h>
#include <string.h>

#include "llex.h"
#include "lmem.h"
#include "lobject.h"
#include "lparser.h"
#include "lstate.h"
#include "lstring.h"
#include "lstx.h"
#include "luadebug.h"
#include "lzio.h"



int lua_debug=0;


#define next(LL) (LL->current = zgetc(LL->lex_z))


static struct {
  char *name;
  int token;
} reserved [] = {
    {"and", AND}, {"do", DO}, {"else", ELSE}, {"elseif", ELSEIF},
    {"end", END}, {"function", FUNCTION}, {"if", IF}, {"local", LOCAL},
    {"nil", NIL}, {"not", NOT}, {"or", OR}, {"repeat", REPEAT},
    {"return", RETURN}, {"then", THEN}, {"until", UNTIL}, {"while", WHILE}
};

void luaX_init (void)
{
  int i;
  for (i=0; i<(sizeof(reserved)/sizeof(reserved[0])); i++) {
    TaggedString *ts = luaS_new(reserved[i].name);
    ts->head.marked = reserved[i].token;  /* reserved word  (always > 255) */
  }
}


static void firstline (LexState *LL)
{
  int c = zgetc(LL->lex_z);
  if (c == '#')
    while((c=zgetc(LL->lex_z)) != '\n' && c != EOZ) /* skip first line */;
  zungetc(LL->lex_z);
}


void luaX_setinput (ZIO *z)
{
  LexState *LL = L->lexstate;
  LL->current = '\n';
  LL->linelasttoken = 0;
  LL->lastline = 0;
  LL->linenumber = 0;
  LL->iflevel = 0;
  LL->ifstate[0].skip = 0;
  LL->ifstate[0].elsepart = 1;  /* to avoid a free $else */
  LL->lex_z = z;
  firstline(LL);
  LL->textbuff.buffsize = 20;
  LL->textbuff.text = luaM_buffer(LL->textbuff.buffsize);
}



/*
** =======================================================
** PRAGMAS
** =======================================================
*/

#define PRAGMASIZE	20

static void skipspace (LexState *LL)
{
  while (LL->current == ' ' || LL->current == '\t') next(LL);
}


static int checkcond (char *buff)
{
  static char *opts[] = {"nil", "1", NULL};
  int i = luaO_findstring(buff, opts);
  if (i >= 0) return i;
  else if (isalpha((unsigned char)buff[0]) || buff[0] == '_')
    return luaS_globaldefined(buff);
  else {
    luaY_syntaxerror("invalid $if condition", buff);
    return 0;  /* to avoid warnings */
  }
}


static void readname (LexState *LL, char *buff)
{
  int i = 0;
  skipspace(LL);
  while (isalnum(LL->current) || LL->current == '_') {
    if (i >= PRAGMASIZE) {
      buff[PRAGMASIZE] = 0;
      luaY_syntaxerror("pragma too long", buff);
    }
    buff[i++] = LL->current;
    next(LL);
  }
  buff[i] = 0;
}


static void inclinenumber (LexState *LL);


static void ifskip (LexState *LL)
{
  while (LL->ifstate[LL->iflevel].skip) {
    if (LL->current == '\n')
      inclinenumber(LL);
    else if (LL->current == EOZ)
      luaY_syntaxerror("input ends inside a $if", "");
    else next(LL);
  }
}


static void inclinenumber (LexState *LL)
{
  static char *pragmas [] =
    {"debug", "nodebug", "endinput", "end", "ifnot", "if", "else", NULL};
  next(LL);  /* skip '\n' */
  ++LL->linenumber;
  if (LL->current == '$') {  /* is a pragma? */
    char buff[PRAGMASIZE+1];
    int ifnot = 0;
    int skip = LL->ifstate[LL->iflevel].skip;
    next(LL);  /* skip $ */
    readname(LL, buff);
    switch (luaO_findstring(buff, pragmas)) {
      case 0:  /* debug */
        if (!skip) lua_debug = 1;
        break;
      case 1:  /* nodebug */
        if (!skip) lua_debug = 0;
        break;
      case 2:  /* endinput */
        if (!skip) {
          LL->current = EOZ;
          LL->iflevel = 0;  /* to allow $endinput inside a $if */
        }
        break;
      case 3:  /* end */
        if (LL->iflevel-- == 0)
          luaY_syntaxerror("unmatched $end", "$end");
        break;
      case 4:  /* ifnot */
        ifnot = 1;
        /* go through */
      case 5:  /* if */
        if (LL->iflevel == MAX_IFS-1)
          luaY_syntaxerror("too many nested `$ifs'", "$if");
        readname(LL, buff);
        LL->iflevel++;
        LL->ifstate[LL->iflevel].elsepart = 0;
        LL->ifstate[LL->iflevel].condition = checkcond(buff) ? !ifnot : ifnot;
        LL->ifstate[LL->iflevel].skip = skip || !LL->ifstate[LL->iflevel].condition;
        break;
      case 6:  /* else */
        if (LL->ifstate[LL->iflevel].elsepart)
          luaY_syntaxerror("unmatched $else", "$else");
        LL->ifstate[LL->iflevel].elsepart = 1;
        LL->ifstate[LL->iflevel].skip = LL->ifstate[LL->iflevel-1].skip ||
                                      LL->ifstate[LL->iflevel].condition;
        break;
      default:
        luaY_syntaxerror("invalid pragma", buff);
    }
    skipspace(LL);
    if (LL->current == '\n')  /* pragma must end with a '\n' ... */
      inclinenumber(LL);
    else if (LL->current != EOZ)  /* or eof */
      luaY_syntaxerror("invalid pragma format", buff);
    ifskip(LL);
  }
}


/*
** =======================================================
** LEXICAL ANALIZER
** =======================================================
*/



static void save (LexState *LL, int c)
{
  if (LL->textbuff.tokensize >= LL->textbuff.buffsize)
    LL->textbuff.text = luaM_buffer(LL->textbuff.buffsize *= 2);
  LL->textbuff.text[LL->textbuff.tokensize++] = c;
}


char *luaX_lasttoken (void)
{
  save(L->lexstate, 0);
  return L->lexstate->textbuff.text;
}


#define save_and_next(LL)  (save(LL, LL->current), next(LL))


static int read_long_string (LexState *LL, YYSTYPE *l)
{
  int cont = 0;
  while (1) {
    switch (LL->current) {
      case EOZ:
        save(LL, 0);
        return WRONGTOKEN;
      case '[':
        save_and_next(LL);
        if (LL->current == '[') {
          cont++;
          save_and_next(LL);
        }
        continue;
      case ']':
        save_and_next(LL);
        if (LL->current == ']') {
          if (cont == 0) goto endloop;
          cont--;
          save_and_next(LL);
        }
        continue;
      case '\n':
        save(LL, '\n');
        inclinenumber(LL);
        continue;
      default:
        save_and_next(LL);
    }
  } endloop:
  save_and_next(LL);  /* pass the second ']' */
  LL->textbuff.text[LL->textbuff.tokensize-2] = 0;  /* erases ']]' */
  l->pTStr = luaS_new(LL->textbuff.text+2);
  LL->textbuff.text[LL->textbuff.tokensize-2] = ']';  /* restores ']]' */
  return STRING;
}


/* to avoid warnings; this declaration cannot be public since YYSTYPE
** cannot be visible in llex.h (otherwise there is an error, since
** the parser body redefines it!)
*/
int luaY_lex (YYSTYPE *l);
int luaY_lex (YYSTYPE *l)
{
  LexState *LL = L->lexstate;
  double a;
  LL->textbuff.tokensize = 0;
  if (lua_debug)
    luaY_codedebugline(LL->linelasttoken);
  LL->linelasttoken = LL->linenumber;
  while (1) {
    switch (LL->current) {
      case '\n':
        inclinenumber(LL);
        LL->linelasttoken = LL->linenumber;
        continue;

      case ' ': case '\t': case '\r':  /* CR: to avoid problems with DOS */
        next(LL);
        continue;

      case '-':
        save_and_next(LL);
        if (LL->current != '-') return '-';
        do { next(LL); } while (LL->current != '\n' && LL->current != EOZ);
        LL->textbuff.tokensize = 0;
        continue;

      case '[':
        save_and_next(LL);
        if (LL->current != '[') return '[';
        else {
          save_and_next(LL);  /* pass the second '[' */
          return read_long_string(LL, l);
        }

      case '=':
        save_and_next(LL);
        if (LL->current != '=') return '=';
        else { save_and_next(LL); return EQ; }

      case '<':
        save_and_next(LL);
        if (LL->current != '=') return '<';
        else { save_and_next(LL); return LE; }

      case '>':
        save_and_next(LL);
        if (LL->current != '=') return '>';
        else { save_and_next(LL); return GE; }

      case '~':
        save_and_next(LL);
        if (LL->current != '=') return '~';
        else { save_and_next(LL); return NE; }

      case '"':
      case '\'': {
        int del = LL->current;
        save_and_next(LL);
        while (LL->current != del) {
          switch (LL->current) {
            case EOZ:
            case '\n':
              save(LL, 0);
              return WRONGTOKEN;
            case '\\':
              next(LL);  /* do not save the '\' */
              switch (LL->current) {
                case 'n': save(LL, '\n'); next(LL); break;
                case 't': save(LL, '\t'); next(LL); break;
                case 'r': save(LL, '\r'); next(LL); break;
                case '\n': save(LL, '\n'); inclinenumber(LL); break;
                default : save_and_next(LL); break;
              }
              break;
            default:
              save_and_next(LL);
          }
        }
        next(LL);  /* skip delimiter */
        save(LL, 0);
        l->pTStr = luaS_new(LL->textbuff.text+1);
        LL->textbuff.text[LL->textbuff.tokensize-1] = del;  /* restore delimiter */
        return STRING;
      }

      case '.':
        save_and_next(LL);
        if (LL->current == '.')
        {
          save_and_next(LL);
          if (LL->current == '.')
          {
            save_and_next(LL);
            return DOTS;   /* ... */
          }
          else return CONC;   /* .. */
        }
        else if (!isdigit(LL->current)) return '.';
        /* LL->current is a digit: goes through to number */
	a=0.0;
        goto fraction;

      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
	a=0.0;
        do {
          a=10.0*a+(LL->current-'0');
          save_and_next(LL);
        } while (isdigit(LL->current));
        if (LL->current == '.') {
          save_and_next(LL);
          if (LL->current == '.') {
            save(LL, 0);
            luaY_error(
              "ambiguous syntax (decimal point x string concatenation)");
          }
        }
      fraction:
	{ double da=0.1;
	  while (isdigit(LL->current))
	  {
            a+=(LL->current-'0')*da;
            da/=10.0;
            save_and_next(LL);
          }
          if (toupper(LL->current) == 'E') {
	    int e=0;
	    int neg;
	    double ea;
            save_and_next(LL);
	    neg=(LL->current=='-');
            if (LL->current == '+' || LL->current == '-') save_and_next(LL);
            if (!isdigit(LL->current)) {
              save(LL, 0); return WRONGTOKEN; }
            do {
              e=10.0*e+(LL->current-'0');
              save_and_next(LL);
            } while (isdigit(LL->current));
	    for (ea=neg?0.1:10.0; e>0; e>>=1)
	    {
	      if (e & 1) a*=ea;
	      ea*=ea;
	    }
          }
          l->vReal = a;
          return NUMBER;
        }

      case EOZ:
        save(LL, 0);
        if (LL->iflevel > 0)
          luaY_error("missing $endif");
        return 0;

      default:
        if (LL->current != '_' && !isalpha(LL->current)) {
          save_and_next(LL);
          return LL->textbuff.text[0];
        }
        else {  /* identifier or reserved word */
          TaggedString *ts;
          do {
            save_and_next(LL);
          } while (isalnum(LL->current) || LL->current == '_');
          save(LL, 0);
          ts = luaS_new(LL->textbuff.text);
          if (ts->head.marked > 255)
            return ts->head.marked;  /* reserved word */
          l->pTStr = ts;
          return NAME;
        }
    }
  }
}

