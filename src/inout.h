/*
** $Id: inout.h,v 1.10 1995/10/17 11:58:41 roberto Exp $
*/


#ifndef inout_h
#define inout_h

#include "types.h"


extern Word lua_linenumber;
extern Bool lua_debug;
extern Word lua_debugline;
extern char *lua_parsedfile;

char *lua_openfile     (char *fn);
void lua_closefile    (void);
void lua_openstring   (char *s);
void lua_closestring  (void);

void    lua_internaldofile (void);
void    lua_internaldostring (void);
void    lua_print      (void);
void    luaI_type       (void);
void    lua_obj2number (void);
void	luaI_error     (void);

#endif
