/*
** $Id: lvm.h,v 1.21 2000/04/19 13:36:25 roberto Exp roberto $
** Lua virtual machine
** See Copyright Notice in lua.h
*/

#ifndef lvm_h
#define lvm_h


#include "ldo.h"
#include "lobject.h"
#include "ltm.h"


#define tonumber(o)   ((ttype(o) != TAG_NUMBER) && (luaV_tonumber(o) != 0))
#define tostring(L,o) ((ttype(o) != TAG_STRING) && (luaV_tostring(L, o) != 0))


void luaV_pack (lua_State *L, StkId firstel, int nvararg, TObject *tab);
int luaV_tonumber (TObject *obj);
int luaV_tostring (lua_State *L, TObject *obj);
void luaV_setn (lua_State *L, Hash *t, int val);
void luaV_gettable (lua_State *L, StkId top);
void luaV_settable (lua_State *L, StkId t, StkId top);
void luaV_getglobal (lua_State *L, TString *s, StkId top);
void luaV_setglobal (lua_State *L, TString *s, StkId top);
StkId luaV_execute (lua_State *L, const Closure *cl, StkId base);
void luaV_Cclosure (lua_State *L, lua_CFunction c, int nelems);
void luaV_Lclosure (lua_State *L, Proto *l, int nelems);
int luaV_lessthan (lua_State *L, const TObject *l, const TObject *r, StkId top);

#endif
