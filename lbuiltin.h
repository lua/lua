/*
** $Id: lbuiltin.h,v 1.9 2000/05/26 19:17:57 roberto Exp roberto $
** Built-in functions
** See Copyright Notice in lua.h
*/

#ifndef lbuiltin_h
#define lbuiltin_h

#include "lua.h"

int luaB__ALERT (lua_State *L);
int luaB__ERRORMESSAGE (lua_State *L);
int luaB_assert (lua_State *L);
int luaB_call (lua_State *L);
int luaB_collectgarbage (lua_State *L);
int luaB_copytagmethods (lua_State *L);
int luaB_dofile (lua_State *L);
int luaB_dostring (lua_State *L);
int luaB_error (lua_State *L);
int luaB_getglobal (lua_State *L);
int luaB_getn (lua_State *L);
int luaB_gettagmethod (lua_State *L);
int luaB_globals (lua_State *L);
int luaB_newtag (lua_State *L);
int luaB_next (lua_State *L);
int luaB_print (lua_State *L);
int luaB_rawget (lua_State *L);
int luaB_rawset (lua_State *L);
int luaB_setglobal (lua_State *L);
int luaB_settag (lua_State *L);
int luaB_settagmethod (lua_State *L);
int luaB_sort (lua_State *L);
int luaB_tag (lua_State *L);
int luaB_tinsert (lua_State *L);
int luaB_tonumber (lua_State *L);
int luaB_tostring (lua_State *L);
int luaB_tremove (lua_State *L);
int luaB_type (lua_State *L);

void luaB_predefine (lua_State *L);


#endif
