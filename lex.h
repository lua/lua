/*
** lex.h
** TecCGraf - PUC-Rio
** $Id: $
*/

#ifndef lex_h
#define lex_h


typedef int  (*Input) (void);

void    lua_setinput   (Input fn);      /* from "lex.c" module */
char   *lua_lasttext   (void);          /* from "lex.c" module */
int     luaY_lex (void);                /* from "lex.c" module */


#endif
