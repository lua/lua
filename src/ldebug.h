/*
** $Id: ldebug.h,v 2.5 2009/06/10 16:57:53 roberto Exp $
** Auxiliary functions from Debug Interface module
** See Copyright Notice in lua.h
*/

#ifndef ldebug_h
#define ldebug_h


#include "lstate.h"


#define pcRel(pc, p)	(cast(int, (pc) - (p)->code) - 1)

#define getfuncline(f,pc)	(((f)->lineinfo) ? (f)->lineinfo[pc] : 0)

#define resethookcount(L)	(L->hookcount = L->basehookcount)


LUAI_FUNC void luaG_typeerror (lua_State *L, const TValue *o,
                                             const char *opname);
LUAI_FUNC void luaG_concaterror (lua_State *L, StkId p1, StkId p2);
LUAI_FUNC void luaG_aritherror (lua_State *L, const TValue *p1,
                                              const TValue *p2);
LUAI_FUNC int luaG_ordererror (lua_State *L, const TValue *p1,
                                             const TValue *p2);
LUAI_FUNC void luaG_runerror (lua_State *L, const char *fmt, ...);
LUAI_FUNC void luaG_errormsg (lua_State *L);

#endif
