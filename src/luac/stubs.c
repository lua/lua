/*
** $Id: stubs.c,v 1.11 1999/03/11 17:09:10 lhf Exp $
** avoid runtime modules in luac
** See Copyright Notice in lua.h
*/

#ifdef NOSTUBS

/* according to gcc, ANSI C forbids an empty source file */
void luaU_dummy(void);
void luaU_dummy(void){}

#else

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
void luaL_verror (char *fmt, ...)
{
  char buff[500];
  va_list argp;
  va_start(argp, fmt);
  vsprintf(buff, fmt, argp);
  va_end(argp);
  lua_error(buff);
}

/* copied from lauxlib.c */
void luaL_filesource (char *out, char *filename, int len) {
  if (filename == NULL) filename = "(stdin)";
  sprintf(out, "@%.*s", len-2, filename);  /* -2 for '@' and '\0' */
}

/* avoid runtime modules in lstate.c */

#include "lbuiltin.h"
#include "ldo.h"
#include "lgc.h"
#include "ltable.h"
#include "ltm.h"

void luaB_predefine(void){}
void luaC_hashcallIM(Hash *l){}
void luaC_strcallIM(TaggedString *l){}
void luaD_gcIM(TObject *o){}
void luaH_free(Hash *frees){}
void luaT_init(void){}

/*
* the code below avoids the lexer and the parser (llex lparser).
* it is useful if you only want to load binary files.
* this works for interpreters like lua.c too.
*/

#ifdef NOPARSER

#include "llex.h"
#include "lparser.h"

void luaX_init(void){}
void luaD_init(void){}

TProtoFunc* luaY_parser(ZIO *z) {
 lua_error("parser not loaded");
 return NULL;
}

#else

/* copied from lauxlib.c */
int luaL_findstring (char *name, char *list[]) {
  int i;
  for (i=0; list[i]; i++)
    if (strcmp(list[i], name) == 0)
      return i;
  return -1;  /* name not found */
}

/* copied from lauxlib.c */
void luaL_chunkid (char *out, char *source, int len) {
  len -= 13;  /* 13 = strlen("string ''...\0") */
  if (*source == '@')
    sprintf(out, "file `%.*s'", len, source+1);
  else if (*source == '(')
    strcpy(out, "(C code)");
  else {
    char *b = strchr(source , '\n');  /* stop string at first new line */
    int lim = (b && (b-source)<len) ? b-source : len;
    sprintf(out, "string `%.*s'", lim, source);
    strcpy(out+lim+(13-5), "...'");  /* 5 = strlen("...'\0") */
  }
}

void luaD_checkstack(int n){}

#define STACK_UNIT	128

/* copied from ldo.c */
void luaD_init (void) {
  L->stack.stack = luaM_newvector(STACK_UNIT, TObject);
  L->stack.top = L->stack.stack;
  L->stack.last = L->stack.stack+(STACK_UNIT-1);
}

#endif
#endif
