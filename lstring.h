/*
** $Id: lstring.h,v 1.8 1999/08/16 20:52:00 roberto Exp roberto $
** String table (keep all strings handled by Lua)
** See Copyright Notice in lua.h
*/

#ifndef lstring_h
#define lstring_h


#include "lobject.h"


#define NUM_HASHSTR     31
#define NUM_HASHUDATA   31
#define NUM_HASHS  (NUM_HASHSTR+NUM_HASHUDATA)


extern TaggedString luaS_EMPTY;

void luaS_init (void);
TaggedString *luaS_createudata (void *udata, int tag);
void luaS_freeall (void);
void luaS_free (TaggedString *ts);
TaggedString *luaS_newlstr (const char *str, long l);
TaggedString *luaS_new (const char *str);
TaggedString *luaS_newfixedstring (const char *str);
void luaS_rawsetglobal (TaggedString *ts, const TObject *newval);
int luaS_globaldefined (const char *name);


#endif
