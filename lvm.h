/*
** $Id: lvm.h,v 1.13 1999/12/01 19:50:08 roberto Exp roberto $
** Lua virtual machine
** See Copyright Notice in lua.h
*/

#ifndef lvm_h
#define lvm_h


#include "ldo.h"
#include "lobject.h"
#include "ltm.h"


#define tonumber(o) ((ttype(o) != LUA_T_NUMBER) && (luaV_tonumber(o) != 0))
#define tostring(L, o) ((ttype(o) != LUA_T_STRING) && (luaV_tostring(L, o) != 0))


void luaV_pack (lua_State *L, StkId firstel, int nvararg, TObject *tab);
int luaV_tonumber (TObject *obj);
int luaV_tostring (lua_State *L, TObject *obj);
void luaV_setn (lua_State *L, Hash *t, int val);
void luaV_gettable (lua_State *L);
void luaV_settable (lua_State *L, StkId t);
void luaV_rawsettable (lua_State *L, StkId t);
void luaV_getglobal (lua_State *L, GlobalVar *gv);
void luaV_setglobal (lua_State *L, GlobalVar *gv);
StkId luaV_execute (lua_State *L, const Closure *cl, const TProtoFunc *tf, StkId base);
void luaV_closure (lua_State *L, int nelems);
void luaV_comparison (lua_State *L);

#endif
