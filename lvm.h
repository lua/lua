/*
** $Id: lvm.h,v 1.10 1999/10/14 19:46:57 roberto Exp roberto $
** Lua virtual machine
** See Copyright Notice in lua.h
*/

#ifndef lvm_h
#define lvm_h


#include "ldo.h"
#include "lobject.h"
#include "ltm.h"


#define tonumber(o) ((ttype(o) != LUA_T_NUMBER) && (luaV_tonumber(o) != 0))
#define tostring(o) ((ttype(o) != LUA_T_STRING) && (luaV_tostring(o) != 0))


void luaV_pack (StkId firstel, int nvararg, TObject *tab);
int luaV_tonumber (TObject *obj);
int luaV_tostring (TObject *obj);
void luaV_setn (Hash *t, int val);
void luaV_gettable (void);
void luaV_settable (const TObject *t);
void luaV_rawsettable (const TObject *t);
void luaV_getglobal (GlobalVar *gv);
void luaV_setglobal (GlobalVar *gv);
StkId luaV_execute (const Closure *cl, const TProtoFunc *tf, StkId base);
void luaV_closure (int nelems);
void luaV_comparison (lua_Type ttype_less, lua_Type ttype_equal,
                      lua_Type ttype_great, IMS op);

#endif
