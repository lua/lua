/*
** $Id: lvm.h,v 1.27 2000/10/05 12:14:08 roberto Exp $
** Lua virtual machine
** See Copyright Notice in lua.h
*/

#ifndef lvm_h
#define lvm_h


#include "ldo.h"
#include "lobject.h"
#include "ltm.h"


#define tonumber(o)   ((ttype(o) != LUA_TNUMBER) && (luaV_tonumber(o) != 0))
#define tostring(L,o) ((ttype(o) != LUA_TSTRING) && (luaV_tostring(L, o) != 0))


int luaV_tonumber (TObject *obj);
int luaV_tostring (lua_State *L, TObject *obj);
const TObject *luaV_gettable (lua_State *L, StkId t);
void luaV_settable (lua_State *L, StkId t, StkId key);
const TObject *luaV_getglobal (lua_State *L, TString *s);
void luaV_setglobal (lua_State *L, TString *s);
StkId luaV_execute (lua_State *L, const Closure *cl, StkId base);
void luaV_Cclosure (lua_State *L, lua_CFunction c, int nelems);
void luaV_Lclosure (lua_State *L, Proto *l, int nelems);
int luaV_lessthan (lua_State *L, const TObject *l, const TObject *r, StkId top);
void luaV_strconc (lua_State *L, int total, StkId top);

#endif
