/*
** $Id: llex.c,v 1.45 1999/12/02 16:41:29 roberto Exp roberto $
** Lexical Analyzer
** See Copyright Notice in lua.h
*/


#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define LUA_REENTRANT

#include "lauxlib.h"
#include "llex.h"
#include "lmem.h"
#include "lobject.h"
#include "lparser.h"
#include "lstate.h"
#include "lstring.h"
#include "luadebug.h"
#include "lzio.h"



#define next(LS) (LS->current = zgetc(LS->lex_z))


#define save(L, c)	luaL_addchar(L, c)
#define save_and_next(L, LS)  (save(L, LS->current), next(LS))


/* ORDER RESERVED */
static const char *const reserved [] = {"and", "do", "else", "elseif", "end",
    "function", "if", "local", "nil", "not", "or", "repeat", "return", "then",
    "until", "while"};


void luaX_init (lua_State *L) {
  unsigned int i;
  for (i=0; i<(sizeof(reserved)/sizeof(reserved[0])); i++) {
    TaggedString *ts = luaS_new(L, reserved[i]);
    ts->marked = (unsigned char)(RESERVEDMARK+i);  /* reserved word */
  }
}


#define MAXSRC          80

void luaX_syntaxerror (LexState *ls, const char *s, const char *token) {
  char buff[MAXSRC];
  luaL_chunkid(buff, zname(ls->lex_z), sizeof(buff));
  if (token[0] == '\0')
    token = "<eof>";
  luaL_verror(ls->L, "%.100s;\n  last token read: `%.50s' at line %d in %.80s",
              s, token, ls->linenumber, buff);
}


void luaX_error (LexState *ls, const char *s) {
  save(ls->L, '\0');
  luaX_syntaxerror(ls, s, luaL_buffer(ls->L));
}


void luaX_token2str (int token, char *s) {
  if (token < 256) {
    s[0] = (char)token;
    s[1] = '\0';
  }
  else
    strcpy(s, reserved[token-FIRST_RESERVED]);
}


static void luaX_invalidchar (LexState *ls, int c) {
  char buff[8];
  sprintf(buff, "0x%02X", c);
  luaX_syntaxerror(ls, "invalid control char", buff);
}


static void firstline (LexState *LS)
{
  int c = zgetc(LS->lex_z);
  if (c == '#')
    while ((c=zgetc(LS->lex_z)) != '\n' && c != EOZ) /* skip first line */;
  zungetc(LS->lex_z);
}


void luaX_setinput (lua_State *L, LexState *LS, ZIO *z) {
  LS->L = L;
  LS->current = '\n';
  LS->linenumber = 0;
  LS->iflevel = 0;
  LS->ifstate[0].skip = 0;
  LS->ifstate[0].elsepart = 1;  /* to avoid a free $else */
  LS->lex_z = z;
  LS->fs = NULL;
  firstline(LS);
  luaL_resetbuffer(L);
}



/*
** =======================================================
** PRAGMAS
** =======================================================
*/

#ifndef PRAGMASIZE
#define PRAGMASIZE	80  /* arbitrary limit */
#endif

static void skipspace (LexState *LS) {
  while (LS->current == ' ' || LS->current == '\t' || LS->current == '\r')
    next(LS);
}


static int checkcond (lua_State *L, LexState *LS, const char *buff) {
  static const char *const opts[] = {"nil", "1", NULL};
  int i = luaL_findstring(buff, opts);
  if (i >= 0) return i;
  else if (isalpha((unsigned char)buff[0]) || buff[0] == '_')
    return luaS_globaldefined(L, buff);
  else {
    luaX_syntaxerror(LS, "invalid $if condition", buff);
    return 0;  /* to avoid warnings */
  }
}


static void readname (LexState *LS, char *buff) {
  int i = 0;
  skipspace(LS);
  while (isalnum(LS->current) || LS->current == '_') {
    if (i >= PRAGMASIZE) {
      buff[PRAGMASIZE] = 0;
      luaX_syntaxerror(LS, "pragma too long", buff);
    }
    buff[i++] = (char)LS->current;
    next(LS);
  }
  buff[i] = 0;
}


static void inclinenumber (lua_State *L, LexState *LS);


static void ifskip (lua_State *L, LexState *LS) {
  while (LS->ifstate[LS->iflevel].skip) {
    if (LS->current == '\n')
      inclinenumber(L, LS);
    else if (LS->current == EOZ)
      luaX_error(LS, "input ends inside a $if");
    else next(LS);
  }
}


static void inclinenumber (lua_State *L, LexState *LS) {
  static const char *const pragmas [] =
    {"debug", "nodebug", "endinput", "end", "ifnot", "if", "else", NULL};
  next(LS);  /* skip '\n' */
  ++LS->linenumber;
  if (LS->current == '$') {  /* is a pragma? */
    char buff[PRAGMASIZE+1];
    int ifnot = 0;
    int skip = LS->ifstate[LS->iflevel].skip;
    next(LS);  /* skip $ */
    readname(LS, buff);
    switch (luaL_findstring(buff, pragmas)) {
      case 0:  /* debug */
        if (!skip) L->debug = 1;
        break;
      case 1:  /* nodebug */
        if (!skip) L->debug = 0;
        break;
      case 2:  /* endinput */
        if (!skip) {
          LS->current = EOZ;
          LS->iflevel = 0;  /* to allow $endinput inside a $if */
        }
        break;
      case 3:  /* end */
        if (LS->iflevel-- == 0)
          luaX_syntaxerror(LS, "unmatched $end", "$end");
        break;
      case 4:  /* ifnot */
        ifnot = 1;
        /* go through */
      case 5:  /* if */
        if (LS->iflevel == MAX_IFS-1)
          luaX_syntaxerror(LS, "too many nested $ifs", "$if");
        readname(LS, buff);
        LS->iflevel++;
        LS->ifstate[LS->iflevel].elsepart = 0;
        LS->ifstate[LS->iflevel].condition = checkcond(L, LS, buff) ? !ifnot : ifnot;
        LS->ifstate[LS->iflevel].skip = skip || !LS->ifstate[LS->iflevel].condition;
        break;
      case 6:  /* else */
        if (LS->ifstate[LS->iflevel].elsepart)
          luaX_syntaxerror(LS, "unmatched $else", "$else");
        LS->ifstate[LS->iflevel].elsepart = 1;
        LS->ifstate[LS->iflevel].skip = LS->ifstate[LS->iflevel-1].skip ||
                                      LS->ifstate[LS->iflevel].condition;
        break;
      default:
        luaX_syntaxerror(LS, "unknown pragma", buff);
    }
    skipspace(LS);
    if (LS->current == '\n')  /* pragma must end with a '\n' ... */
      inclinenumber(L, LS);
    else if (LS->current != EOZ)  /* or eof */
      luaX_syntaxerror(LS, "invalid pragma format", buff);
    ifskip(L, LS);
  }
}



/*
** =======================================================
** LEXICAL ANALYZER
** =======================================================
*/



static int read_long_string (lua_State *L, LexState *LS) {
  int cont = 0;
  for (;;) {
    switch (LS->current) {
      case EOZ:
        luaX_error(LS, "unfinished long string");
        return EOS;  /* to avoid warnings */
      case '[':
        save_and_next(L, LS);
        if (LS->current == '[') {
          cont++;
          save_and_next(L, LS);
        }
        continue;
      case ']':
        save_and_next(L, LS);
        if (LS->current == ']') {
          if (cont == 0) goto endloop;
          cont--;
          save_and_next(L, LS);
        }
        continue;
      case '\n':
        save(L, '\n');
        inclinenumber(L, LS);
        continue;
      default:
        save_and_next(L, LS);
    }
  } endloop:
  save_and_next(L, LS);  /* skip the second ']' */
  LS->seminfo.ts = luaS_newlstr(L, L->Mbuffer+(L->Mbuffbase+2),
                          L->Mbuffnext-L->Mbuffbase-4);
  return STRING;
}


int luaX_lex (LexState *LS) {
  lua_State *L = LS->L;
  luaL_resetbuffer(L);
  for (;;) {
    switch (LS->current) {

      case ' ': case '\t': case '\r':  /* CR: to avoid problems with DOS */
        next(LS);
        continue;

      case '\n':
        inclinenumber(L, LS);
        continue;

      case '-':
        save_and_next(L, LS);
        if (LS->current != '-') return '-';
        do { next(LS); } while (LS->current != '\n' && LS->current != EOZ);
        luaL_resetbuffer(L);
        continue;

      case '[':
        save_and_next(L, LS);
        if (LS->current != '[') return '[';
        else {
          save_and_next(L, LS);  /* pass the second '[' */
          return read_long_string(L, LS);
        }

      case '=':
        save_and_next(L, LS);
        if (LS->current != '=') return '=';
        else { save_and_next(L, LS); return EQ; }

      case '<':
        save_and_next(L, LS);
        if (LS->current != '=') return '<';
        else { save_and_next(L, LS); return LE; }

      case '>':
        save_and_next(L, LS);
        if (LS->current != '=') return '>';
        else { save_and_next(L, LS); return GE; }

      case '~':
        save_and_next(L, LS);
        if (LS->current != '=') return '~';
        else { save_and_next(L, LS); return NE; }

      case '"':
      case '\'': {
        int del = LS->current;
        save_and_next(L, LS);
        while (LS->current != del) {
          switch (LS->current) {
            case EOZ:
            case '\n':
              luaX_error(LS, "unfinished string");
              return EOS;  /* to avoid warnings */
            case '\\':
              next(LS);  /* do not save the '\' */
              switch (LS->current) {
                case 'a': save(L, '\a'); next(LS); break;
                case 'b': save(L, '\b'); next(LS); break;
                case 'f': save(L, '\f'); next(LS); break;
                case 'n': save(L, '\n'); next(LS); break;
                case 'r': save(L, '\r'); next(LS); break;
                case 't': save(L, '\t'); next(LS); break;
                case 'v': save(L, '\v'); next(LS); break;
                case '\n': save(L, '\n'); inclinenumber(L, LS); break;
                default : {
                  if (isdigit(LS->current)) {
                    int c = 0;
                    int i = 0;
                    do {
                      c = 10*c + (LS->current-'0');
                      next(LS);
                    } while (++i<3 && isdigit(LS->current));
                    if (c != (unsigned char)c)
                      luaX_error(LS, "escape sequence too large");
                    save(L, c);
                  }
                  else {  /* handles \, ", ', and ? */
                    save(L, LS->current);
                    next(LS);
                  }
                  break;
                }
              }
              break;
            default:
              save_and_next(L, LS);
          }
        }
        save_and_next(L, LS);  /* skip delimiter */
        LS->seminfo.ts = luaS_newlstr(L, L->Mbuffer+(L->Mbuffbase+1),
                                L->Mbuffnext-L->Mbuffbase-2);
        return STRING;
      }

      case '.':
        save_and_next(L, LS);
        if (LS->current == '.')
        {
          save_and_next(L, LS);
          if (LS->current == '.')
          {
            save_and_next(L, LS);
            return DOTS;   /* ... */
          }
          else return CONC;   /* .. */
        }
        else if (!isdigit(LS->current)) return '.';
        goto fraction;  /* LS->current is a digit: goes through to number */

      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
        do {
          save_and_next(L, LS);
        } while (isdigit(LS->current));
        if (LS->current == '.') {
          save_and_next(L, LS);
          if (LS->current == '.') {
            save(L, '.');
            luaX_error(LS, 
              "ambiguous syntax (decimal point x string concatenation)");
          }
        }
      fraction:  /* LUA_NUMBER */
        while (isdigit(LS->current))
          save_and_next(L, LS);
        if (toupper(LS->current) == 'E') {
          save_and_next(L, LS);  /* read 'E' */
          save_and_next(L, LS);  /* read '+', '-' or first digit */
          while (isdigit(LS->current))
            save_and_next(L, LS);
        }
        save(L, '\0');
        if (!luaO_str2d(L->Mbuffer+L->Mbuffbase, &LS->seminfo.r))
          luaX_error(LS, "malformed number");
        return NUMBER;

      case EOZ:
        if (LS->iflevel > 0)
          luaX_error(LS, "input ends inside a $if");
        return EOS;

      default:
        if (LS->current != '_' && !isalpha(LS->current)) {
          int c = LS->current;
          if (iscntrl(c))
            luaX_invalidchar(LS, c);
          save_and_next(L, LS);
          return c;
        }
        else {  /* identifier or reserved word */
          TaggedString *ts;
          do {
            save_and_next(L, LS);
          } while (isalnum(LS->current) || LS->current == '_');
          save(L, '\0');
          ts = luaS_new(L, L->Mbuffer+L->Mbuffbase);
          if (ts->marked >= RESERVEDMARK)  /* reserved word? */
            return ts->marked-RESERVEDMARK+FIRST_RESERVED;
          LS->seminfo.ts = ts;
          return NAME;
        }
    }
  }
}

