/*
** $Id: inout.h,v 1.8 1995/05/02 18:43:03 roberto Exp roberto $
*/


#ifndef inout_h
#define inout_h

#include "types.h"


extern Word lua_linenumber;
extern Bool lua_debug;
extern Word lua_debugline;

char *lua_openfile     (char *fn);
void lua_closefile    (void);
char *lua_openstring   (char *s);
void lua_closestring  (void);
void lua_pushfunction (char *file, Word function);
void lua_popfunction  (void);
void luaI_reportbug (char *s, int err);

void    lua_internaldofile (void);
void    lua_internaldostring (void);
void    lua_print      (void);
void    luaI_type       (void);
void    lua_obj2number (void);
void	luaI_getstack  (void);
void	luaI_error     (void);

#endif
