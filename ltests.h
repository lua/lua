/*
** $Id: ltests.h,v 1.4 2001/02/06 18:18:58 roberto Exp roberto $
** Internal Header for Debugging of the Lua Implementation
** See Copyright Notice in lua.h
*/

#ifndef ltests_h
#define ltests_h


#include <stdlib.h>


#define LUA_DEBUG

#undef NDEBUG
#include <assert.h>
#define lua_assert(c)           assert(c)
#define api_check(L, o)		lua_assert(o)


/* to avoid warnings, and to make sure value is really unused */
#define UNUSED(x)       (x=0, (void)(x))


/* memory allocator control variables */
extern unsigned long memdebug_numblocks;
extern unsigned long memdebug_total;
extern unsigned long memdebug_maxmem;
extern unsigned long memdebug_memlimit;


#define l_malloc(s)		debug_realloc(NULL, 0, s)
#define l_realloc(b, os, s)	debug_realloc(b, os, s)
#define l_free(b, s)		debug_realloc(b, s, 0)

void *debug_realloc (void *block, size_t oldsize, size_t size);



/* test for lock/unlock */
#define LUA_USERSTATE	int *lock;
extern int islocked;
#define LUA_LOCK(L)     lua_assert((**((int **)L))++ == 0)
#define LUA_UNLOCK(L)   lua_assert(--(**((int **)L)) == 0)


extern lua_State *lua_state;


void luaB_opentests (lua_State *L);

#define LUA_USERINIT(L) (luaB_opentests(L), openstdlibs(L))


#endif
