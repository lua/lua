/*
** $Id: $
** Lua virtual machine
** See Copyright Notice in lua.h
*/

#ifndef lvm_h
#define lvm_h


#include "ldo.h"
#include "lobject.h"


#define tonumber(o) ((ttype(o) != LUA_T_NUMBER) && (luaV_tonumber(o) != 0))
#define tostring(o) ((ttype(o) != LUA_T_STRING) && (luaV_tostring(o) != 0))


void luaV_pack (StkId firstel, int nvararg, TObject *tab);
int luaV_tonumber (TObject *obj);
int luaV_tostring (TObject *obj);
void luaV_gettable (void);
void luaV_settable (TObject *t, int mode);
void luaV_getglobal (Word n);
void luaV_setglobal (Word n);
StkId luaV_execute (Closure *func, StkId base);
void luaV_closure (void);

#endif
