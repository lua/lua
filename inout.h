/*
** $Id: inout.h,v 1.16 1996/05/28 21:07:32 roberto Exp roberto $
*/


#ifndef inout_h
#define inout_h

#include "types.h"
#include <stdio.h>


extern Word lua_linenumber;
extern Word lua_debugline;
extern char *lua_parsedfile;

FILE *lua_openfile     (char *fn);
void lua_closefile    (void);
void lua_openstring   (char *s);
void lua_closestring  (void);

void luaI_predefine (void);

#endif
