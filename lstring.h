/*
** $Id: lstring.h,v 1.3 1997/11/04 15:27:53 roberto Exp roberto $
** String table (keep all strings handled by Lua)
** See Copyright Notice in lua.h
*/

#ifndef lstring_h
#define lstring_h


#include "lobject.h"


void luaS_init (void);
TaggedString *luaS_createudata (void *udata, int tag);
TaggedString *luaS_collector (void);
void luaS_free (TaggedString *l);
void luaS_callIM (TaggedString *l);
TaggedString *luaS_new (char *str);
TaggedString *luaS_newfixedstring (char *str);
void luaS_rawsetglobal (TaggedString *ts, TObject *newval);
char *luaS_travsymbol (int (*fn)(TObject *));
int luaS_globaldefined (char *name);

#endif
