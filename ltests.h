/*
** $Id: ltests.h,v 1.7 2001/06/28 19:58:57 roberto Exp $
** Internal Header for Debugging of the Lua Implementation
** See Copyright Notice in lua.h
*/

#ifndef ltests_h
#define ltests_h


#include <stdlib.h>


#define LUA_DEBUG

#define LUA_OPNAMES

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
#define lua_lock(L)     lua_assert((**cast(int **, L))++ == 0)
#define lua_unlock(L)   lua_assert(--(**cast(int **, L)) == 0)


extern lua_State *lua_state;


void luaB_opentests (lua_State *L);

#define LUA_USERINIT(L) (luaB_opentests(L), openstdlibs(L))


#endif
