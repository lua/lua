/*
** $Id: lstring.h,v 1.7 1998/03/06 16:54:42 roberto Exp roberto $
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
TaggedString *luaS_newlstr (const char *str, long l);
TaggedString *luaS_new (const char *str);
TaggedString *luaS_newfixedstring (const char *str);
void luaS_rawsetglobal (TaggedString *ts, TObject *newval);
const char *luaS_travsymbol (int (*fn)(TObject *));
int luaS_globaldefined (const char *name);
TaggedString *luaS_collectudata (void);
void luaS_freeall (void);


#endif
