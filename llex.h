/*
** $Id: $
** Lexical Analizer
** See Copyright Notice in lua.h
*/

#ifndef llex_h
#define llex_h

#include "lobject.h"
#include "lzio.h"


extern int luaX_linenumber;

int  luaY_lex (void);
void luaX_setinput (ZIO *z);
char *luaX_lasttoken (void);


#endif
