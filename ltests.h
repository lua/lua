/*
** $Id: $
** Internal Header for Debugging of the Lua Implementation
** See Copyright Notice in lua.h
*/

#ifndef ltests_h
#define ltests_h


#include "llimits.h"


#define LUA_DEBUG

#undef NDEBUG
#include <assert.h>
#define lua_assert(c)           assert(c)


/* to avoid warnings, and to make sure value is really unused */
#define UNUSED(x)       (x=0, (void)(x))


/* memory allocator control variables */
extern mem_int memdebug_numblocks;
extern mem_int memdebug_total;
extern mem_int memdebug_maxmem;
extern mem_int memdebug_memlimit;


/* test for lock/unlock */
#define LUA_USERSTATE	int *lock;
extern int islocked;
#define LUA_LOCK(L)     lua_assert((**((int **)L))++ == 0)
#define LUA_UNLOCK(L)   lua_assert(--(**((int **)L)) == 0)


extern lua_State *lua_state;


void luaB_opentests (lua_State *L);

#define LUA_USERINIT(L) luaB_opentests(L)


#endif
