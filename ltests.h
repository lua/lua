/*
** $Id: ltests.h,v 2.9 2004/07/16 13:17:00 roberto Exp roberto $
** Internal Header for Debugging of the Lua Implementation
** See Copyright Notice in lua.h
*/

#ifndef ltests_h
#define ltests_h


#include <stdlib.h>


#define LUA_DEBUG

#undef NDEBUG
#include <assert.h>
#undef lua_assert
#define lua_assert(c)           assert(c)


/* to avoid warnings, and to make sure value is really unused */
#define UNUSED(x)       (x=0, (void)(x))


/* memory allocator control variables */
typedef struct Memcontrol {
  unsigned long numblocks;
  unsigned long total;
  unsigned long maxmem;
  unsigned long memlimit;
} Memcontrol;

extern Memcontrol memcontrol;


/*
** generic variable for debug tricks
*/
extern int Trick;


void *debug_realloc (void *ud, void *block, size_t osize, size_t nsize);

#ifdef lua_c
#define luaL_newstate()	lua_newstate(debug_realloc, &memcontrol)
#endif


int lua_checkmemory (lua_State *L);


/* test for lock/unlock */
#undef lua_userstateopen
#undef lua_lock
#undef lua_unlock

extern int islocked;
#define LUA_USERSTATE	int *
#define getlock(l)	(*(cast(LUA_USERSTATE *, l) - 1))
#define lua_userstateopen(l)	getlock(l) = &islocked;
#define lua_lock(l)     lua_assert((*getlock(l))++ == 0)
#define lua_unlock(l)   lua_assert(--(*getlock(l)) == 0)


int luaB_opentests (lua_State *L);

#undef lua_userinit
#define lua_userinit(L)	{ luaopen_stdlibs(L); luaB_opentests(L); }



/* real main will be defined at `ltests.c' */
int l_main (int argc, char *argv[]);
#define main	l_main



/* change some sizes to give some bugs a chance */

#undef LUAL_BUFFERSIZE
#define LUAL_BUFFERSIZE		27
#define MINSTRTABSIZE		2

#endif
