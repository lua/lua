/*
** $Id: inout.h,v 1.11 1996/01/22 17:40:00 roberto Exp roberto $
*/


#ifndef inout_h
#define inout_h

#include "types.h"


extern Word lua_linenumber;
extern Bool lua_debug;
extern Word lua_debugline;
extern char *lua_parsedfile;

int  lua_openfile     (char *fn);
void lua_closefile    (void);
void lua_openstring   (char *s);
void lua_closestring  (void);

void    lua_internaldofile (void);
void    lua_internaldostring (void);
void    luaI_tostring   (void);
void    luaI_print      (void);
void    luaI_type       (void);
void    lua_obj2number (void);
void	luaI_error     (void);
void    luaI_assert    (void);
void	luaI_setglobal (void);
void	luaI_getglobal (void);

#endif
