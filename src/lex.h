/*
** lex.h
** TecCGraf - PUC-Rio
** $Id: lex.h,v 1.4 1997/06/16 16:50:22 roberto Exp $
*/

#ifndef lex_h
#define lex_h

#include "zio.h"

void    lua_setinput   (ZIO *z);
void luaI_syntaxerror (char *s);
int     luaY_lex (void);
void luaI_addReserved (void);


#endif
