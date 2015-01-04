/*
** $Id: llex.c,v 1.72 2000/10/20 16:39:03 roberto Exp $
** Lexical Analyzer
** See Copyright Notice in lua.h
*/


#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "lua.h"

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
  luaO_chunkid(buff, ls->source->str, sizeof(buff));
  luaO_verror(ls->L, "%.99s;\n  last token read: `%.30s' at line %d in %.80s",
              s, token, ls->linenumber, buff);
}


void luaX_error (LexState *ls, const char *s, int token) {
  char buff[TOKEN_LEN];
  luaX_token2str(token, buff);
  if (buff[0] == '\0')
    luaX_syntaxerror(ls, s, ls->L->Mbuffer);
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


static void inclinenumber (LexState *LS) {
  next(LS);  /* skip '\n' */
  ++LS->linenumber;
  luaX_checklimit(LS, LS->linenumber, MAX_INT, "lines in a chunk");
}


void luaX_setinput (lua_State *L, LexState *LS, ZIO *z, TString *source) {
  LS->L = L;
  LS->lookahead.token = TK_EOS;  /* no look-ahead token */
  LS->z = z;
  LS->fs = NULL;
  LS->linenumber = 1;
  LS->lastline = 1;
  LS->source = source;
  next(LS);  /* read first char */
  if (LS->current == '#') {
    do {  /* skip first line */
      next(LS);
    } while (LS->current != '\n' && LS->current != EOZ);
  }
}



/*
** =======================================================
** LEXICAL ANALYZER
** =======================================================
*/


/* use Mbuffer to store names, literal strings and numbers */

#define EXTRABUFF	128
#define checkbuffer(L, n, len)	if ((len)+(n) > L->Mbuffsize) \
                                  luaO_openspace(L, (len)+(n)+EXTRABUFF)

#define save(L, c, l)	(L->Mbuffer[l++] = (char)c)
#define save_and_next(L, LS, l)  (save(L, LS->current, l), next(LS))


static const char *readname (LexState *LS) {
  lua_State *L = LS->L;
  size_t l = 0;
  checkbuffer(L, 10, l);
  do {
    checkbuffer(L, 10, l);
    save_and_next(L, LS, l);
  } while (isalnum(LS->current) || LS->current == '_');
  save(L, '\0', l);
  return L->Mbuffer;
}


/* LUA_NUMBER */
static void read_number (LexState *LS, int comma, SemInfo *seminfo) {
  lua_State *L = LS->L;
  size_t l = 0;
  checkbuffer(L, 10, l);
  if (comma) save(L, '.', l);
  while (isdigit(LS->current)) {
    checkbuffer(L, 10, l);
    save_and_next(L, LS, l);
  }
  if (LS->current == '.') {
    save_and_next(L, LS, l);
    if (LS->current == '.') {
      save_and_next(L, LS, l);
      save(L, '\0', l);
      luaX_error(LS, "ambiguous syntax"
           " (decimal point x string concatenation)", TK_NUMBER);
    }
  }
  while (isdigit(LS->current)) {
    checkbuffer(L, 10, l);
    save_and_next(L, LS, l);
  }
  if (LS->current == 'e' || LS->current == 'E') {
    save_and_next(L, LS, l);  /* read 'E' */
    if (LS->current == '+' || LS->current == '-')
      save_and_next(L, LS, l);  /* optional exponent sign */
    while (isdigit(LS->current)) {
      checkbuffer(L, 10, l);
      save_and_next(L, LS, l);
    }
  }
  save(L, '\0', l);
  if (!luaO_str2d(L->Mbuffer, &seminfo->r))
    luaX_error(LS, "malformed number", TK_NUMBER);
}


static void read_long_string (LexState *LS, SemInfo *seminfo) {
  lua_State *L = LS->L;
  int cont = 0;
  size_t l = 0;
  checkbuffer(L, 10, l);
  save(L, '[', l);  /* save first '[' */
  save_and_next(L, LS, l);  /* pass the second '[' */
  for (;;) {
    checkbuffer(L, 10, l);
    switch (LS->current) {
      case EOZ:
        save(L, '\0', l);
        luaX_error(LS, "unfinished long string", TK_STRING);
        break;  /* to avoid warnings */
      case '[':
        save_and_next(L, LS, l);
        if (LS->current == '[') {
          cont++;
          save_and_next(L, LS, l);
        }
        continue;
      case ']':
        save_and_next(L, LS, l);
        if (LS->current == ']') {
          if (cont == 0) goto endloop;
          cont--;
          save_and_next(L, LS, l);
        }
        continue;
      case '\n':
        save(L, '\n', l);
        inclinenumber(LS);
        continue;
      default:
        save_and_next(L, LS, l);
    }
  } endloop:
  save_and_next(L, LS, l);  /* skip the second ']' */
  save(L, '\0', l);
  seminfo->ts = luaS_newlstr(L, L->Mbuffer+2, l-5);
}


static void read_string (LexState *LS, int del, SemInfo *seminfo) {
  lua_State *L = LS->L;
  size_t l = 0;
  checkbuffer(L, 10, l);
  save_and_next(L, LS, l);
  while (LS->current != del) {
    checkbuffer(L, 10, l);
    switch (LS->current) {
      case EOZ:  case '\n':
        save(L, '\0', l);
        luaX_error(LS, "unfinished string", TK_STRING);
        break;  /* to avoid warnings */
      case '\\':
        next(LS);  /* do not save the '\' */
        switch (LS->current) {
          case 'a': save(L, '\a', l); next(LS); break;
          case 'b': save(L, '\b', l); next(LS); break;
          case 'f': save(L, '\f', l); next(LS); break;
          case 'n': save(L, '\n', l); next(LS); break;
          case 'r': save(L, '\r', l); next(LS); break;
          case 't': save(L, '\t', l); next(LS); break;
          case 'v': save(L, '\v', l); next(LS); break;
          case '\n': save(L, '\n', l); inclinenumber(LS); break;
          case '0': case '1': case '2': case '3': case '4':
          case '5': case '6': case '7': case '8': case '9': {
            int c = 0;
            int i = 0;
            do {
              c = 10*c + (LS->current-'0');
              next(LS);
            } while (++i<3 && isdigit(LS->current));
            if (c != (unsigned char)c) {
              save(L, '\0', l);
              luaX_error(LS, "escape sequence too large", TK_STRING);
            }
            save(L, c, l);
            break;
          }
          default:  /* handles \\, \", \', and \? */
            save_and_next(L, LS, l);
        }
        break;
      default:
        save_and_next(L, LS, l);
    }
  }
  save_and_next(L, LS, l);  /* skip delimiter */
  save(L, '\0', l);
  seminfo->ts = luaS_newlstr(L, L->Mbuffer+1, l-3);
}


int luaX_lex (LexState *LS, SemInfo *seminfo) {
  for (;;) {
    switch (LS->current) {

      case ' ': case '\t': case '\r':  /* `\r' to avoid problems with DOS */
        next(LS);
        continue;

      case '\n':
        inclinenumber(LS);
        continue;

      case '$':
        luaX_error(LS, "unexpected `$' (pragmas are no longer supported)", '$');
        break;

      case '-':
        next(LS);
        if (LS->current != '-') return '-';
        do { next(LS); } while (LS->current != '\n' && LS->current != EOZ);
        continue;

      case '[':
        next(LS);
        if (LS->current != '[') return '[';
        else {
          read_long_string(LS, seminfo);
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
        read_string(LS, LS->current, seminfo);
        return TK_STRING;

      case '.':
        next(LS);
        if (LS->current == '.') {
          next(LS);
          if (LS->current == '.') {
            next(LS);
            return TK_DOTS;   /* ... */
          }
          else return TK_CONCAT;   /* .. */
        }
        else if (!isdigit(LS->current)) return '.';
        else {
          read_number(LS, 1, seminfo);
          return TK_NUMBER;
        }

      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
        read_number(LS, 0, seminfo);
        return TK_NUMBER;

      case EOZ:
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
          TString *ts = luaS_new(LS->L, readname(LS));
          if (ts->marked >= RESERVEDMARK)  /* reserved word? */
            return ts->marked-RESERVEDMARK+FIRST_RESERVED;
          seminfo->ts = ts;
          return TK_NAME;
        }
    }
  }
}

