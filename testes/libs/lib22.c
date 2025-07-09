/* implementation for lib2-v2 */

#include <string.h>

#include "lua.h"
#include "lauxlib.h"

static int id (lua_State *L) {
  lua_pushboolean(L, 1);
  lua_insert(L, 1);
  return lua_gettop(L);
}


struct STR {
  void *ud;
  lua_Alloc allocf;
};


static void *t_freestr (void *ud, void *ptr, size_t osize, size_t nsize) {
  struct STR *blk = (struct STR*)ptr - 1;
  blk->allocf(blk->ud, blk, sizeof(struct STR) + osize, 0);
  return NULL;
}


static int newstr (lua_State *L) {
  size_t len;
  const char *str = luaL_checklstring(L, 1, &len);
  void *ud;
  lua_Alloc allocf = lua_getallocf(L, &ud);
  struct STR *blk = (struct STR*)allocf(ud, NULL, 0,
                                        len + 1 + sizeof(struct STR));
  if (blk == NULL) {  /* allocation error? */
    lua_pushliteral(L, "not enough memory");
    lua_error(L);  /* raise a memory error */
  }
  blk->ud = ud;  blk->allocf = allocf;
  memcpy(blk + 1, str, len + 1);
  lua_pushexternalstring(L, (char *)(blk + 1), len, t_freestr, L);
  return 1;
}


/*
** Create an external string and keep it in the registry, so that it
** will test that the library code is still available (to deallocate
** this string) when closing the state.
*/
static void initstr (lua_State *L) {
  lua_pushcfunction(L, newstr);
  lua_pushstring(L,
     "012345678901234567890123456789012345678901234567890123456789");
  lua_call(L, 1, 1);  /* call newstr("0123...") */
  luaL_ref(L, LUA_REGISTRYINDEX);  /* keep string in the registry */
}


static const struct luaL_Reg funcs[] = {
  {"id", id},
  {"newstr", newstr},
  {NULL, NULL}
};


LUAMOD_API int luaopen_lib2 (lua_State *L) {
  lua_settop(L, 2);
  lua_setglobal(L, "y");  /* y gets 2nd parameter */
  lua_setglobal(L, "x");  /* x gets 1st parameter */
  initstr(L);
  luaL_newlib(L, funcs);
  return 1;
}


