/*
** $Id: ldebug.h,v 1.19 2002/04/10 12:11:07 roberto Exp roberto $
** Auxiliary functions from Debug Interface module
** See Copyright Notice in lua.h
*/

#ifndef ldebug_h
#define ldebug_h


#include "lstate.h"
#include "luadebug.h"


#define pcRel(pc, p)	(cast(int, (pc) - (p)->code) - 1)

#define getline(f,pc)	(((f)->lineinfo) ? (f)->lineinfo[pc] : 0)

void luaG_typeerror (lua_State *L, const TObject *o, const char *opname);
void luaG_concaterror (lua_State *L, StkId p1, StkId p2);
void luaG_aritherror (lua_State *L, StkId p1, const TObject *p2);
void luaG_ordererror (lua_State *L, const TObject *p1, const TObject *p2);
int luaG_checkcode (const Proto *pt);


#endif
