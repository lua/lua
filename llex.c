/*
** $Id: llex.c,v 1.59 2000/05/24 13:54:49 roberto Exp roberto $
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
#include "ltable.h"
#include "luadebug.h"
#include "lzio.h"



#define next(LS) (LS->current = zgetc(LS->z))


#define save(L, c)	luaL_addchar(L, c)
#define save_and_next(L, LS)  (save(L, LS->current), next(LS))


/* ORDER RESERVED */
static const char *const token2string [] = {
    "and", "break", "do", "else", "elseif", "end", "for",
    "function", "if", "local", "nil", "not", "or", "repeat", "return", "then",
    "until", "while", "", "..", "...", "==", ">=", "<=", "~=", "", "", "<eof>"};


void luaX_init (lua_State *L) {
  int i;
  for (i=0; i<NUM_RESERVED; i++) {
    TString *ts = luaS_new(L, token2string[i]);
    ts->marked = (unsigned char)(RESERVEDMARK+i);  /* reserved word */
  }
}


#define MAXSRC          80


void luaX_checklimit (LexState *ls, int val, int limit, const char *msg) {
  if (val > limit) {
    char buff[100];
    sprintf(buff, "too many %.50s (limit=%d)", msg, limit);
    luaX_error(ls, buff, ls->t.token);
  }
}


void luaX_syntaxerror (LexState *ls, const char *s, const char *token) {
  char buff[MAXSRC];
  luaL_chunkid(buff, zname(ls->z), sizeof(buff));
  luaL_verror(ls->L, "%.100s;\n  last token read: `%.50s' at line %d in %.80s",
              s, token, ls->linenumber, buff);
}


void luaX_error (LexState *ls, const char *s, int token) {
  char buff[TOKEN_LEN];
  luaX_token2str(token, buff);
  if (buff[0] == '\0') {
    save(ls->L, '\0');
    luaX_syntaxerror(ls, s, luaL_buffer(ls->L));
  }
  else
    luaX_syntaxerror(ls, s, buff);
}


void luaX_token2str (int token, char *s) {
  if (token < 256) {
    s[0] = (char)token;
    s[1] = '\0';
  }
  else
    strcpy(s, token2string[token-FIRST_RESERVED]);
}


static void luaX_invalidchar (LexState *ls, int c) {
  char buff[8];
  sprintf(buff, "0x%02X", c);
  luaX_syntaxerror(ls, "invalid control char", buff);
}


static void firstline (LexState *LS)
{
  int c = zgetc(LS->z);
  if (c == '#')
    while ((c=zgetc(LS->z)) != '\n' && c != EOZ) /* skip first line */;
  zungetc(LS->z);
}


void luaX_setinput (lua_State *L, LexState *LS, ZIO *z) {
  LS->L = L;
  LS->current = '\n';
  LS->next.token = TK_EOS;  /* no next token */
  LS->linenumber = 0;
  LS->iflevel = 0;
  LS->ifstate[0].skip = 0;
  LS->ifstate[0].elsepart = 1;  /* to avoid a free $else */
  LS->z = z;
  LS->fs = NULL;
  firstline(LS);
  luaL_resetbuffer(L);
}



/*
** =======================================================
** PRAGMAS
** =======================================================
*/

static void skipspace (LexState *LS) {
  while (LS->current == ' ' || LS->current == '\t' || LS->current == '\r')
    next(LS);
}


static int globaldefined (lua_State *L, const char *name) {
  const TObject *value = luaH_getglobal(L, name);
  return ttype(value) != TAG_NIL;
}


static int checkcond (lua_State *L, LexState *LS, const char *buff) {
  static const char *const opts[] = {"nil", "1", NULL};
  int i = luaL_findstring(buff, opts);
  if (i >= 0) return i;
  else if (isalpha((unsigned char)buff[0]) || buff[0] == '_')
    return globaldefined(L, buff);
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
      luaX_error(LS, "input ends inside a $if", TK_EOS);
    else next(LS);
  }
}


static void inclinenumber (lua_State *L, LexState *LS) {
  static const char *const pragmas [] =
    {"debug", "nodebug", "endinput", "end", "ifnot", "if", "else", NULL};
  next(LS);  /* skip '\n' */
  ++LS->linenumber;
  luaX_checklimit(LS, LS->linenumber, MAX_INT, "lines in a chunk");
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



static void read_long_string (lua_State *L, LexState *LS) {
  int cont = 0;
  for (;;) {
    switch (LS->current) {
      case EOZ:
        luaX_error(LS, "unfinished long string", TK_STRING);
        break;  /* to avoid warnings */
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
  LS->t.seminfo.ts = luaS_newlstr(L, L->Mbuffer+(L->Mbuffbase+2),
                          L->Mbuffnext-L->Mbuffbase-4);
}


static void read_string (lua_State *L, LexState *LS, int del) {
  save_and_next(L, LS);
  while (LS->current != del) {
    switch (LS->current) {
      case EOZ:  case '\n':
        luaX_error(LS, "unfinished string", TK_STRING);
        break;  /* to avoid warnings */
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
          case '0': case '1': case '2': case '3': case '4':
          case '5': case '6': case '7': case '8': case '9': {
            int c = 0;
            int i = 0;
            do {
              c = 10*c + (LS->current-'0');
              next(LS);
            } while (++i<3 && isdigit(LS->current));
            if (c != (unsigned char)c)
              luaX_error(LS, "escape sequence too large", TK_STRING);
            save(L, c);
            break;
          }
          default:  /* handles \\, \", \', and \? */
            save(L, LS->current);
            next(LS);
        }
        break;
      default:
        save_and_next(L, LS);
    }
  }
  save_and_next(L, LS);  /* skip delimiter */
  LS->t.seminfo.ts = luaS_newlstr(L, L->Mbuffer+(L->Mbuffbase+1),
                          L->Mbuffnext-L->Mbuffbase-2);
}


int luaX_lex (LexState *LS) {
  lua_State *L = LS->L;
  for (;;) {
    switch (LS->current) {

      case ' ': case '\t': case '\r':  /* `\r' to avoid problems with DOS */
        next(LS);
        continue;

      case '\n':
        inclinenumber(L, LS);
        continue;

      case '-':
        next(LS);
        if (LS->current != '-') return '-';
        do { next(LS); } while (LS->current != '\n' && LS->current != EOZ);
        continue;

      case '[':
        luaL_resetbuffer(L);
        save_and_next(L, LS);
        if (LS->current != '[') return '[';
        else {
          save_and_next(L, LS);  /* pass the second '[' */
          read_long_string(L, LS);
          return TK_STRING;
        }

      case '=':
        next(LS);
        if (LS->current != '=') return '=';
        else { next(LS); return TK_EQ; }

      case '<':
        next(LS);
        if (LS->current != '=') return '<';
        else { next(LS); return TK_LE; }

      case '>':
        next(LS);
        if (LS->current != '=') return '>';
        else { next(LS); return TK_GE; }

      case '~':
        next(LS);
        if (LS->current != '=') return '~';
        else { next(LS); return TK_NE; }

      case '"':
      case '\'':
        luaL_resetbuffer(L);
        read_string(L, LS, LS->current);
        return TK_STRING;

      case '.':
        luaL_resetbuffer(L);
        save_and_next(L, LS);
        if (LS->current == '.') {
          next(LS);
          if (LS->current == '.') {
            next(LS);
            return TK_DOTS;   /* ... */
          }
          else return TK_CONCAT;   /* .. */
        }
        else if (!isdigit(LS->current)) return '.';
        else goto fraction;  /* LS->current is a digit */

      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
        luaL_resetbuffer(L);
        do {
          save_and_next(L, LS);
        } while (isdigit(LS->current));
        if (LS->current == '.') {
          save_and_next(L, LS);
          if (LS->current == '.') {
            save(L, '.');
            luaX_error(LS, "ambiguous syntax"
                       " (decimal point x string concatenation)", TK_NUMBER);
          }
        }
      fraction:  /* LUA_NUMBER */
        while (isdigit(LS->current))
          save_and_next(L, LS);
        if (LS->current == 'e' || LS->current == 'E') {
          save_and_next(L, LS);  /* read 'E' */
          if (LS->current == '+' || LS->current == '-')
            save_and_next(L, LS);  /* optional exponent sign */
          while (isdigit(LS->current))
            save_and_next(L, LS);
        }
        save(L, '\0');
        if (!luaO_str2d(L->Mbuffer+L->Mbuffbase, &LS->t.seminfo.r))
          luaX_error(LS, "malformed number", TK_NUMBER);
        return TK_NUMBER;

      case EOZ:
        if (LS->iflevel > 0)
          luaX_error(LS, "input ends inside a $if", TK_EOS);
        return TK_EOS;

      case '_': goto tname;

      default:
        if (!isalpha(LS->current)) {
          int c = LS->current;
          if (iscntrl(c))
            luaX_invalidchar(LS, c);
          next(LS);
          return c;
        }
        tname: {  /* identifier or reserved word */
          TString *ts;
          luaL_resetbuffer(L);
          do {
            save_and_next(L, LS);
          } while (isalnum(LS->current) || LS->current == '_');
          save(L, '\0');
          ts = luaS_new(L, L->Mbuffer+L->Mbuffbase);
          if (ts->marked >= RESERVEDMARK)  /* reserved word? */
            return ts->marked-RESERVEDMARK+FIRST_RESERVED;
          LS->t.seminfo.ts = ts;
          return TK_NAME;
        }
    }
  }
}

