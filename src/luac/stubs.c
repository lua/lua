/*
** $Id: stubs.c,v 1.8 1998/07/12 00:17:37 lhf Exp $
** avoid runtime modules in luac
** See Copyright Notice in lua.h
*/

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "luac.h"

/*
* avoid lapi lauxlib lbuiltin ldo lgc ltable ltm lvm
* use only lbuffer lfunc llex lmem lobject lparser lstate lstring lzio
*/

/* simplified from ldo.c */
void lua_error(char* s)
{
 if (s) fprintf(stderr,"luac: %s\n",s);
 exit(1);
}

/* copied from lauxlib.c */
void luaL_verror(char* fmt, ...)
{
 char buff[500];
 va_list argp;
 va_start(argp,fmt);
 vsprintf(buff,fmt,argp);
 va_end(argp);
 lua_error(buff);
}

/* copied from lauxlib.c */
int luaL_findstring (char* name, char* list[])
{
 int i;
 for (i=0; list[i]; i++)
   if (strcmp(list[i], name) == 0)
     return i;
 return -1;
}

/* avoid runtime modules in lstate.c */
void luaB_predefine(void){}
void luaC_hashcallIM(Hash *l){}
void luaC_strcallIM(TaggedString *l){}
void luaD_gcIM(TObject *o){}
void luaD_init(void){}
void luaH_free(Hash *frees){}
void luaT_init(void){}

/*
* the code below avoids the lexer and the parser (llex lparser).
* it is useful if you only want to load binary files.
* this works for interpreters like lua.c too.
*/

#ifdef NOPARSER

int lua_debug=0;

void luaX_init(void){}
void luaY_init(void){}
void luaY_parser(void) { lua_error("parser not loaded"); }

#endif
