/*
** $Id: llex.c,v 1.87 2001/06/15 20:36:57 roberto Exp roberto $
** Lexical Analyzer
** See Copyright Notice in lua.h
*/


#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define LUA_PRIVATE
#include "lua.h"

#include "llex.h"
#include "lobject.h"
#include "lparser.h"
#include "lstate.h"
#include "lstring.h"
#include "lzio.h"



#define next(LS) (LS->current = zgetc(LS->z))



/* ORDER RESERVED */
static const l_char *const token2string [] = {
    l_s("and"), l_s("break"), l_s("do"), l_s("else"), l_s("elseif"),
    l_s("end"), l_s("for"), l_s("function"), l_s("global"), l_s("if"),
    l_s("in"), l_s("local"), l_s("nil"), l_s("not"), l_s("or"), l_s("repeat"),
    l_s("return"), l_s("then"), l_s("until"), l_s("while"), l_s(""),
    l_s(".."), l_s("..."), l_s("=="), l_s(">="), l_s("<="), l_s("~="),
    l_s(""), l_s(""), l_s("<eof>")
};


void luaX_init (lua_State *L) {
  int i;
  for (i=0; i<NUM_RESERVED; i++) {
    TString *ts = luaS_new(L, token2string[i]);
    lua_assert(strlen(token2string[i])+1 <= TOKEN_LEN);
    ts->tsv.marked = (unsigned short)(RESERVEDMARK+i);  /* reserved word */
  }
}


#define MAXSRC          80


void luaX_checklimit (LexState *ls, int val, int limit, const l_char *msg) {
  if (val > limit) {
    l_char buff[90];
    sprintf(buff, l_s("too many %.40s (limit=%d)"), msg, limit);
    luaX_error(ls, buff, ls->t.token);
  }
}


void luaX_syntaxerror (LexState *ls, const l_char *s, const l_char *token) {
  l_char buff[MAXSRC];
  luaO_chunkid(buff, getstr(ls->source), MAXSRC);
  luaO_verror(ls->L,
              l_s("%.99s;\n  last token read: `%.30s' at line %d in %.80s"),
              s, token, ls->linenumber, buff);
}


void luaX_error (LexState *ls, const l_char *s, int token) {
  l_char buff[TOKEN_LEN];
  luaX_token2str(token, buff);
  if (buff[0] == l_c('\0'))
    luaX_syntaxerror(ls, s, (l_char *)G(ls->L)->Mbuffer);
  else
    luaX_syntaxerror(ls, s, buff);
}


void luaX_token2str (int token, l_char *s) {
  if (token < 256) {
    s[0] = (l_char)token;
    s[1] = l_c('\0');
  }
  else
    strcpy(s, token2string[token-FIRST_RESERVED]);
}


static void luaX_invalidchar (LexState *ls, int c) {
  l_char buff[8];
  sprintf(buff, l_s("0x%02X"), uchar(c));
  luaX_syntaxerror(ls, l_s("invalid control char"), buff);
}


static void inclinenumber (LexState *LS) {
  next(LS);  /* skip `\n' */
  ++LS->linenumber;
  luaX_checklimit(LS, LS->linenumber, MAX_INT, l_s("lines in a chunk"));
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
  if (LS->current == l_c('#')) {
    do {  /* skip first line */
      next(LS);
    } while (LS->current != l_c('\n') && LS->current != EOZ);
  }
}



/*
** =======================================================
** LEXICAL ANALYZER
** =======================================================
*/


/* use Mbuffer to store names, literal strings and numbers */

#define EXTRABUFF	128
#define checkbuffer(L, n, len)	\
    if (((len)+(n))*sizeof(l_char) > G(L)->Mbuffsize) \
      luaO_openspace(L, (len)+(n)+EXTRABUFF, l_char)

#define save(L, c, l)	(((l_char *)G(L)->Mbuffer)[l++] = (l_char)c)
#define save_and_next(L, LS, l)  (save(L, LS->current, l), next(LS))


static size_t readname (LexState *LS) {
  lua_State *L = LS->L;
  size_t l = 0;
  checkbuffer(L, 10, l);
  do {
    checkbuffer(L, 10, l);
    save_and_next(L, LS, l);
  } while (isalnum(LS->current) || LS->current == l_c('_'));
  save(L, l_c('\0'), l);
  return l-1;
}


/* LUA_NUMBER */
static void read_number (LexState *LS, int comma, SemInfo *seminfo) {
  lua_State *L = LS->L;
  size_t l = 0;
  checkbuffer(L, 10, l);
  if (comma) save(L, l_c('.'), l);
  while (isdigit(LS->current)) {
    checkbuffer(L, 10, l);
    save_and_next(L, LS, l);
  }
  if (LS->current == l_c('.')) {
    save_and_next(L, LS, l);
    if (LS->current == l_c('.')) {
      save_and_next(L, LS, l);
      save(L, l_c('\0'), l);
      luaX_error(LS,
                 l_s("ambiguous syntax (decimal point x string concatenation)"),
                 TK_NUMBER);
    }
  }
  while (isdigit(LS->current)) {
    checkbuffer(L, 10, l);
    save_and_next(L, LS, l);
  }
  if (LS->current == l_c('e') || LS->current == l_c('E')) {
    save_and_next(L, LS, l);  /* read `E' */
    if (LS->current == l_c('+') || LS->current == l_c('-'))
      save_and_next(L, LS, l);  /* optional exponent sign */
    while (isdigit(LS->current)) {
      checkbuffer(L, 10, l);
      save_and_next(L, LS, l);
    }
  }
  save(L, l_c('\0'), l);
  if (!luaO_str2d((l_char *)G(L)->Mbuffer, &seminfo->r))
    luaX_error(LS, l_s("malformed number"), TK_NUMBER);
}


static void read_long_string (LexState *LS, SemInfo *seminfo) {
  lua_State *L = LS->L;
  int cont = 0;
  size_t l = 0;
  checkbuffer(L, 10, l);
  save(L, l_c('['), l);  /* save first `[' */
  save_and_next(L, LS, l);  /* pass the second `[' */
  if (LS->current == l_c('\n'))  /* string starts with a newline? */
    inclinenumber(LS);  /* skip it */
  for (;;) {
    checkbuffer(L, 10, l);
    switch (LS->current) {
      case EOZ:
        save(L, l_c('\0'), l);
        luaX_error(LS, l_s("unfinished long string"), TK_STRING);
        break;  /* to avoid warnings */
      case l_c('['):
        save_and_next(L, LS, l);
        if (LS->current == l_c('[')) {
          cont++;
          save_and_next(L, LS, l);
        }
        continue;
      case l_c(']'):
        save_and_next(L, LS, l);
        if (LS->current == l_c(']')) {
          if (cont == 0) goto endloop;
          cont--;
          save_and_next(L, LS, l);
        }
        continue;
      case l_c('\n'):
        save(L, l_c('\n'), l);
        inclinenumber(LS);
        continue;
      default:
        save_and_next(L, LS, l);
    }
  } endloop:
  save_and_next(L, LS, l);  /* skip the second `]' */
  save(L, l_c('\0'), l);
  seminfo->ts = luaS_newlstr(L, (l_char *)G(L)->Mbuffer+2, l-5);
}


static void read_string (LexState *LS, int del, SemInfo *seminfo) {
  lua_State *L = LS->L;
  size_t l = 0;
  checkbuffer(L, 10, l);
  save_and_next(L, LS, l);
  while (LS->current != del) {
    checkbuffer(L, 10, l);
    switch (LS->current) {
      case EOZ:  case l_c('\n'):
        save(L, l_c('\0'), l);
        luaX_error(LS, l_s("unfinished string"), TK_STRING);
        break;  /* to avoid warnings */
      case l_c('\\'):
        next(LS);  /* do not save the `\' */
        switch (LS->current) {
          case l_c('a'): save(L, l_c('\a'), l); next(LS); break;
          case l_c('b'): save(L, l_c('\b'), l); next(LS); break;
          case l_c('f'): save(L, l_c('\f'), l); next(LS); break;
          case l_c('n'): save(L, l_c('\n'), l); next(LS); break;
          case l_c('r'): save(L, l_c('\r'), l); next(LS); break;
          case l_c('t'): save(L, l_c('\t'), l); next(LS); break;
          case l_c('v'): save(L, l_c('\v'), l); next(LS); break;
          case l_c('\n'): save(L, l_c('\n'), l); inclinenumber(LS); break;
          default: {
            if (!isdigit(LS->current))
              save_and_next(L, LS, l);  /* handles \\, \", \', and \? */
            else {  /* \xxx */
              int c = 0;
              int i = 0;
              do {
                c = 10*c + (LS->current-l_c('0'));
                next(LS);
              } while (++i<3 && isdigit(LS->current));
              if (c > UCHAR_MAX) {
                save(L, l_c('\0'), l);
                luaX_error(LS, l_s("escape sequence too large"), TK_STRING);
              }
              save(L, c, l);
            }
          }
        }
        break;
      default:
        save_and_next(L, LS, l);
    }
  }
  save_and_next(L, LS, l);  /* skip delimiter */
  save(L, l_c('\0'), l);
  seminfo->ts = luaS_newlstr(L, (l_char *)G(L)->Mbuffer+1, l-3);
}


int luaX_lex (LexState *LS, SemInfo *seminfo) {
  for (;;) {
    switch (LS->current) {

      case l_c(' '): case l_c('\t'): case l_c('\r'):  /* `\r' to avoid problems with DOS */
        next(LS);
        continue;

      case l_c('\n'):
        inclinenumber(LS);
        continue;

      case l_c('$'):
        luaX_error(LS,
                   l_s("unexpected `$' (pragmas are no longer supported)"),
                   LS->current);
        break;

      case l_c('-'):
        next(LS);
        if (LS->current != l_c('-')) return l_c('-');
        do { next(LS); } while (LS->current != l_c('\n') && LS->current != EOZ);
        continue;

      case l_c('['):
        next(LS);
        if (LS->current != l_c('[')) return l_c('[');
        else {
          read_long_string(LS, seminfo);
          return TK_STRING;
        }

      case l_c('='):
        next(LS);
        if (LS->current != l_c('=')) return l_c('=');
        else { next(LS); return TK_EQ; }

      case l_c('<'):
        next(LS);
        if (LS->current != l_c('=')) return l_c('<');
        else { next(LS); return TK_LE; }

      case l_c('>'):
        next(LS);
        if (LS->current != l_c('=')) return l_c('>');
        else { next(LS); return TK_GE; }

      case l_c('~'):
        next(LS);
        if (LS->current != l_c('=')) return l_c('~');
        else { next(LS); return TK_NE; }

      case l_c('"'):
      case l_c('\''):
        read_string(LS, LS->current, seminfo);
        return TK_STRING;

      case l_c('.'):
        next(LS);
        if (LS->current == l_c('.')) {
          next(LS);
          if (LS->current == l_c('.')) {
            next(LS);
            return TK_DOTS;   /* ... */
          }
          else return TK_CONCAT;   /* .. */
        }
        else if (!isdigit(LS->current)) return l_c('.');
        else {
          read_number(LS, 1, seminfo);
          return TK_NUMBER;
        }

      case EOZ:
        return TK_EOS;

      default: {
        if (isdigit(LS->current)) {
          read_number(LS, 0, seminfo);
          return TK_NUMBER;
        }
        else if (isalpha(LS->current) || LS->current == l_c('_')) {
          /* identifier or reserved word */
          size_t l = readname(LS);
          TString *ts = luaS_newlstr(LS->L, (l_char *)G(LS->L)->Mbuffer, l);
          if (ts->tsv.marked >= RESERVEDMARK)  /* reserved word? */
            return ts->tsv.marked-RESERVEDMARK+FIRST_RESERVED;
          seminfo->ts = ts;
          return TK_NAME;
        }
        else {
          int c = LS->current;
          if (iscntrl(c))
            luaX_invalidchar(LS, c);
          next(LS);
          return c;  /* single-char tokens (+ - / ...) */
        }
      }
    }
  }
}

