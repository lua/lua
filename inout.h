/*
** $Id: inout.h,v 1.17 1997/02/26 17:38:41 roberto Unstable roberto $
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
