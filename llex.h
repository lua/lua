/*
** $Id: llex.h,v 1.4 1997/11/26 18:53:45 roberto Exp roberto $
** Lexical Analizer
** See Copyright Notice in lua.h
*/

#ifndef llex_h
#define llex_h

#include "lobject.h"
#include "lzio.h"


#define MAX_IFS 5

/* "ifstate" keeps the state of each nested $if the lexical is dealing with. */

struct ifState {
  int elsepart;  /* true if its in the $else part */
  int condition;  /* true if $if condition is true */
  int skip;  /* true if part must be skiped */
};

struct textBuff {
  char *text;  /* always points to luaM_buffer */
  int tokensize;
  int buffsize;
};


typedef struct LexState {
  int current;  /* look ahead character */
  struct zio *lex_z;  /* input stream */
  int linenumber;  /* input line counter */
  struct textBuff textbuff;  /* buffer for tokens */
  int linelasttoken;  /* line where last token was read */
  int lastline;  /* last line wherein a SETLINE was generated */
  struct ifState ifstate[MAX_IFS];
  int iflevel;  /* level of nested $if's (for lexical analysis) */
} LexState;


void luaX_init (void);
void luaX_setinput (ZIO *z);
char *luaX_lasttoken (void);


#endif
