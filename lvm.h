/*
** $Id: lvm.h,v 1.38 2002/03/19 12:45:25 roberto Exp roberto $
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


int luaV_cmp (lua_State *L, const TObject *l, const TObject *r, int cond);
const TObject *luaV_tonumber (const TObject *obj, TObject *n);
int luaV_tostring (lua_State *L, TObject *obj);
void luaV_gettable (lua_State *L, const TObject *t, TObject *key, StkId res);
void luaV_settable (lua_State *L, const TObject *t, TObject *key, StkId val);
StkId luaV_execute (lua_State *L);
void luaV_strconc (lua_State *L, int total, int last);

#endif
