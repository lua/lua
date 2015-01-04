/*
** lex.h
** TecCGraf - PUC-Rio
** $Id: lex.h,v 1.3 1996/11/08 12:49:35 roberto Exp $
*/

#ifndef lex_h
#define lex_h


typedef int  (*Input) (void);

void    lua_setinput   (Input fn);
void luaI_syntaxerror (char *s);
int     luaY_lex (void);
void luaI_addReserved (void);


#endif
