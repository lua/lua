/*
** $Id: inout.h,v 1.20 1997/06/19 18:04:34 roberto Exp $
*/


#ifndef inout_h
#define inout_h

#include "types.h"
#include <stdio.h>


extern Word lua_linenumber;
extern Word lua_debugline;
extern char *lua_parsedfile;

void luaI_setparsedfile (char *name);

void luaI_predefine (void);

int lua_dobuffer (char *buff, int size);
int lua_doFILE (FILE *f, int bin);


#endif
