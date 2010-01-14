/*
** $Id: lbitlib.c,v 1.3 2010/01/12 19:40:02 roberto Exp $
** Standard library for bitwise operations
** See Copyright Notice in lua.h
*/

#define lbitlib_c
#define LUA_LIB

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"


/* number of bits considered when shifting/rotating (must be a power of 2) */
#define NBITS	32


typedef LUA_INT32 b_int;
typedef unsigned LUA_INT32 b_uint;


static b_uint getuintarg (lua_State *L, int arg) {
  b_uint r;
  lua_Number x = lua_tonumber(L, arg);
  if (x == 0) luaL_checktype(L, arg, LUA_TNUMBER);
  lua_number2uint(r, x);
  return r;
}


static b_uint andaux (lua_State *L) {
  int i, n = lua_gettop(L);
  b_uint r = ~(b_uint)0;
  for (i = 1; i <= n; i++)
    r &= getuintarg(L, i);
  return r;
}


static int b_and (lua_State *L) {
  b_uint r = andaux(L);
  lua_pushnumber(L, lua_uint2number(r));
  return 1;
}


static int b_test (lua_State *L) {
  b_uint r = andaux(L);
  lua_pushboolean(L, r != 0);
  return 1;
}


static int b_or (lua_State *L) {
  int i, n = lua_gettop(L);
  b_uint r = 0;
  for (i = 1; i <= n; i++)
    r |= getuintarg(L, i);
  lua_pushnumber(L, lua_uint2number(r));
  return 1;
}


static int b_xor (lua_State *L) {
  int i, n = lua_gettop(L);
  b_uint r = 0;
  for (i = 1; i <= n; i++)
    r ^= getuintarg(L, i);
  lua_pushnumber(L, lua_uint2number(r));
  return 1;
}


static int b_not (lua_State *L) {
  b_uint r = ~getuintarg(L, 1);
  lua_pushnumber(L, lua_uint2number(r));
  return 1;
}


static int b_shift (lua_State *L) {
  b_uint r = getuintarg(L, 1);
  lua_Integer i = luaL_checkinteger(L, 2);
  if (i < 0) {  /* shift right? */
    i = -i;
    if (i >= NBITS) r = 0;
    else r >>= i;
  }
  else {  /* shift left */
    if (i >= NBITS) r = 0;
    else r <<= i;
  }
  lua_pushnumber(L, lua_uint2number(r));
  return 1;
}


static int b_rotate (lua_State *L) {
  b_uint r = getuintarg(L, 1);
  lua_Integer i = luaL_checkinteger(L, 2);
  i &= (NBITS - 1);  /* i = i % NBITS */
  r = (r << i) | (r >> (NBITS - i));
  lua_pushnumber(L, lua_uint2number(r));
  return 1;
}


static const luaL_Reg bitlib[] = {
  {"band", b_and},
  {"btest", b_test},
  {"bor", b_or},
  {"bxor", b_xor},
  {"bnot", b_not},
  {"bshift", b_shift},
  {"brotate", b_rotate},
  {NULL, NULL}
};



LUAMOD_API int luaopen_bit (lua_State *L) {
  luaL_register(L, LUA_BITLIBNAME, bitlib);
  return 1;
}
