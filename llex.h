/*
** $Id: llex.h,v 1.2 1997/11/04 15:27:53 roberto Exp roberto $
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
  char *text;
  int tokensize;
  int buffsize;
};


typedef struct LexState {
  int current;  /* look ahead character */
  struct zio *lex_z;
  int linenumber;
  struct ifState ifstate[MAX_IFS];
  int iflevel;  /* level of nested $if's (for lexical analysis) */
  struct textBuff textbuff;
  int linelasttoken;
  int lastline;
} LexState;


extern int luaX_linenumber;


void luaX_init (void);
void luaX_setinput (ZIO *z);
char *luaX_lasttoken (void);


#endif
