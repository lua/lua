/*
** $Id: inout.h,v 1.6 1994/11/21 21:41:09 roberto Stab roberto $
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
void lua_reportbug    (char *s);

void    lua_internaldofile (void);
void    lua_internaldostring (void);
void    lua_print      (void);
void    luaI_type       (void);
void    lua_obj2number (void);
void	luaI_error     (void);

#endif
