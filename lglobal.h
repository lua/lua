/*
** $Id: $
** Global variables
** See Copyright Notice in lua.h
*/

#ifndef lglobal_h
#define lglobal_h


#include "lobject.h"


typedef struct {
 TObject object;
 TaggedString *varname;
} Symbol;


extern Symbol *luaG_global;  /* global variables */
extern int luaG_nglobal;  /* number of global variable (for luac) */


Word luaG_findsymbolbyname (char *name);
Word luaG_findsymbol (TaggedString *t);
int luaG_globaldefined (char *name);
int luaG_nextvar (Word next);
char *luaG_travsymbol (int (*fn)(TObject *));


#define s_object(i)     (luaG_global[i].object)
#define s_ttype(i)	(ttype(&s_object(i)))


#endif
