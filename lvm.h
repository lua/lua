/*
** $Id: lvm.h,v 1.31 2001/09/07 17:39:10 roberto Exp $
** Lua virtual machine
** See Copyright Notice in lua.h
*/

#ifndef lvm_h
#define lvm_h


#include "ldo.h"
#include "lobject.h"
#include "ltm.h"


#define tostring(L,o) ((ttype(o) != LUA_TSTRING) && (luaV_tostring(L, o) != 0))


const TObject *luaV_tonumber (const TObject *obj, TObject *n);
int luaV_tostring (lua_State *L, TObject *obj);
void luaV_gettable (lua_State *L, StkId t, TObject *key, StkId res);
void luaV_settable (lua_State *L, StkId t, TObject *key, StkId val);
void luaV_getglobal (lua_State *L, TString *s, StkId res);
void luaV_setglobal (lua_State *L, TString *s, StkId val);
StkId luaV_execute (lua_State *L, const LClosure *cl, StkId base);
int luaV_lessthan (lua_State *L, const TObject *l, const TObject *r);
void luaV_strconc (lua_State *L, int total, StkId top);

#endif
