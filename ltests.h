/*
** $Id: ltests.h,v 1.16 2002/10/08 18:45:07 roberto Exp roberto $
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
#define check_exp(c,e)		(lua_assert(c), (e))
#define api_check(L, o)		lua_assert(o)


/* to avoid warnings, and to make sure value is really unused */
#define UNUSED(x)       (x=0, (void)(x))


/* memory allocator control variables */
extern unsigned long memdebug_numblocks;
extern unsigned long memdebug_total;
extern unsigned long memdebug_maxmem;
extern unsigned long memdebug_memlimit;


#define l_realloc(b, os, s)	debug_realloc(b, os, s)
#define l_free(b, os)		debug_realloc(b, os, 0)

void *debug_realloc (void *block, size_t oldsize, size_t size);



/* test for lock/unlock */
extern int islocked;
#define LUA_USERSTATE	int *
#define getlock(l)	(*(cast(int **, l) - 1))
#define lua_userstateopen(l) if (l != NULL) getlock(l) = &islocked;
#define lua_lock(l)     lua_assert((*getlock(l))++ == 0)
#define lua_unlock(l)   lua_assert(--(*getlock(l)) == 0)


int luaB_opentests (lua_State *L);

#define lua_userinit(L) (luaB_opentests(L) + openstdlibs(L))



/* change some sizes to give some bugs a chance */

#define LUAL_BUFFERSIZE		27
#define MINSTRTABSIZE		2

#endif
