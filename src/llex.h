/*
** $Id: llex.h,v 1.31 2000/09/27 17:41:58 roberto Exp $
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
  TK_AND = FIRST_RESERVED, TK_BREAK,
  TK_DO, TK_ELSE, TK_ELSEIF, TK_END, TK_FOR, TK_FUNCTION, TK_IF, TK_LOCAL,
  TK_NIL, TK_NOT, TK_OR, TK_REPEAT, TK_RETURN, TK_THEN, TK_UNTIL, TK_WHILE,
  /* other terminal symbols */
  TK_NAME, TK_CONCAT, TK_DOTS, TK_EQ, TK_GE, TK_LE, TK_NE, TK_NUMBER,
  TK_STRING, TK_EOS
};

/* number of reserved words */
#define NUM_RESERVED	((int)(TK_WHILE-FIRST_RESERVED+1))


typedef union {
  Number r;
  TString *ts;
} SemInfo;  /* semantics information */


typedef struct Token {
  int token;
  SemInfo seminfo;
} Token;


typedef struct LexState {
  int current;  /* current character */
  Token t;  /* current token */
  Token lookahead;  /* look ahead token */
  struct FuncState *fs;  /* `FuncState' is private to the parser */
  struct lua_State *L;
  struct zio *z;  /* input stream */
  int linenumber;  /* input line counter */
  int lastline;  /* line of last token `consumed' */
  TString *source;  /* current source name */
} LexState;


void luaX_init (lua_State *L);
void luaX_setinput (lua_State *L, LexState *LS, ZIO *z, TString *source);
int luaX_lex (LexState *LS, SemInfo *seminfo);
void luaX_checklimit (LexState *ls, int val, int limit, const char *msg);
void luaX_syntaxerror (LexState *ls, const char *s, const char *token);
void luaX_error (LexState *ls, const char *s, int token);
void luaX_token2str (int token, char *s);


#endif
