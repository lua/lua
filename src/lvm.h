/*
** $Id: lvm.h,v 2.2 2004/05/14 19:25:09 roberto Exp $
** Lua virtual machine
** See Copyright Notice in lua.h
*/

#ifndef lvm_h
#define lvm_h


#include "ldo.h"
#include "lobject.h"
#include "ltm.h"


#define tostring(L,o) ((ttype(o) == LUA_TSTRING) || (luaV_tostring(L, o)))

#define tonumber(o,n)	(ttype(o) == LUA_TNUMBER || \
                         (((o) = luaV_tonumber(o,n)) != NULL))

#define equalobj(L,o1,o2) \
	(ttype(o1) == ttype(o2) && luaV_equalval(L, o1, o2))


int luaV_lessthan (lua_State *L, const TValue *l, const TValue *r);
int luaV_equalval (lua_State *L, const TValue *t1, const TValue *t2);
const TValue *luaV_tonumber (const TValue *obj, TValue *n);
int luaV_tostring (lua_State *L, StkId obj);
StkId luaV_gettable (lua_State *L, const TValue *t, TValue *key, StkId val,
                     const Instruction *pc);
StkId luaV_settable (lua_State *L, const TValue *t, TValue *key, StkId val,
                     const Instruction *pc);
StkId luaV_execute (lua_State *L, int nexeccalls);
void luaV_concat (lua_State *L, int total, int last);

#endif
