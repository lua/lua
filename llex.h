/*
** $Id: llex.h,v 1.1 1997/09/16 19:25:59 roberto Exp roberto $
** Lexical Analizer
** See Copyright Notice in lua.h
*/

#ifndef llex_h
#define llex_h

#include "lobject.h"
#include "lzio.h"


extern int luaX_linenumber;


void luaX_init (void);
int  luaY_lex (void);
void luaX_setinput (ZIO *z);
char *luaX_lasttoken (void);


#endif
