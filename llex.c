/*
** $Id: llex.c,v 1.67 2000/08/09 19:16:57 roberto Exp roberto $
** Lexical Analyzer
** See Copyright Notice in lua.h
*/


#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "lua.h"

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
  luaL_chunkid(buff, ls->source->str, sizeof(buff));
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


static const char *readname (lua_State *L, LexState *LS) {
  luaL_resetbuffer(L);
  do {
    save_and_next(L, LS);
  } while (isalnum(LS->current) || LS->current == '_');
  save(L, '\0');
  return L->Mbuffer+L->Mbuffbase;
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
        inclinenumber(LS);
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
          case '\n': save(L, '\n'); inclinenumber(LS); break;
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
          TString *ts = luaS_new(L, readname(L, LS));
          if (ts->marked >= RESERVEDMARK)  /* reserved word? */
            return ts->marked-RESERVEDMARK+FIRST_RESERVED;
          LS->t.seminfo.ts = ts;
          return TK_NAME;
        }
    }
  }
}

