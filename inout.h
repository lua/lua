/*
** $Id: inout.h,v 1.18 1997/06/16 16:50:22 roberto Exp roberto $
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

int lua_dobuffer (char *buff, int size);
int lua_doFILE (FILE *f, int bin);


#endif
