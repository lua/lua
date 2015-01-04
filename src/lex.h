/*
** lex.h
** TecCGraf - PUC-Rio
** $Id: lex.h,v 1.2 1996/02/14 13:35:51 roberto Exp $
*/

#ifndef lex_h
#define lex_h


typedef int  (*Input) (void);

void    lua_setinput   (Input fn);
char   *lua_lasttext   (void);
int     luaY_lex (void);
void luaI_addReserved (void);


#endif
