/*
** $Id: llex.h,v 1.18 2000/02/08 16:34:31 roberto Exp roberto $
** Lexical Analyzer
** See Copyright Notice in lua.h
*/

#ifndef llex_h
#define llex_h

#include "lobject.h"
#include "lzio.h"


#define FIRST_RESERVED	257

/* maximum length of a reserved word (+1 for final 0) */
#define TOKEN_LEN	15


/*
* WARNING: if you change the order of this enumeration,
* grep "ORDER RESERVED"
*/
enum RESERVED {
  /* terminal symbols denoted by reserved words */
  AND = FIRST_RESERVED,
  DO, ELSE, ELSEIF, END, FUNCTION, IF, LOCAL, NIL, NOT, OR,
  REPEAT, RETURN, THEN, UNTIL, WHILE,
  /* other terminal symbols */
  NAME, CONC, DOTS, EQ, GE, LE, NE, NUMBER, STRING, EOS
};

/* number of reserved words */
#define NUM_RESERVED	((int)(WHILE-FIRST_RESERVED+1))


#ifndef MAX_IFS
#define MAX_IFS 5  /* arbitrary limit */
#endif

/* `ifState' keeps the state of each nested $if the lexical is dealing with. */

struct ifState {
  int elsepart;  /* true if it's in the $else part */
  int condition;  /* true if $if condition is true */
  int skip;  /* true if part must be skipped */
};


typedef struct LexState {
  int current;  /* look ahead character */
  int token;  /* look ahead token */
  struct FuncState *fs;  /* `FuncState' is private to the parser */
  struct lua_State *L;
  union {
    real r;
    TaggedString *ts;
  } seminfo;  /* semantics information */
  struct zio *z;  /* input stream */
  int linenumber;  /* input line counter */
  int iflevel;  /* level of nested $if's (for lexical analysis) */
  struct ifState ifstate[MAX_IFS];
} LexState;


void luaX_init (lua_State *L);
void luaX_setinput (lua_State *L, LexState *LS, ZIO *z);
int luaX_lex (LexState *LS);
void luaX_syntaxerror (LexState *ls, const char *s, const char *token);
void luaX_error (LexState *ls, const char *s, int token);
void luaX_token2str (int token, char *s);


#endif
