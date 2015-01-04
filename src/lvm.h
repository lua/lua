/*
** $Id: lvm.h,v 1.8 1999/02/08 17:07:59 roberto Exp $
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
void luaV_settable (TObject *t);
void luaV_rawsettable (TObject *t);
void luaV_getglobal (TaggedString *ts);
void luaV_setglobal (TaggedString *ts);
StkId luaV_execute (Closure *cl, TProtoFunc *tf, StkId base);
void luaV_closure (int nelems);
void luaV_comparison (lua_Type ttype_less, lua_Type ttype_equal,
                      lua_Type ttype_great, IMS op);

#endif
