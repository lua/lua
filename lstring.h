/*
** $Id: lstring.h,v 1.1 1997/09/16 19:25:59 roberto Exp roberto $
** String table (keep all strings handled by Lua)
** See Copyright Notice in lua.h
*/

#ifndef lstring_h
#define lstring_h


#include "lobject.h"

extern GCnode luaS_root;

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
