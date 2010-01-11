/*
** $Id: ltests.h,v 2.29 2009/12/17 12:26:09 roberto Exp roberto $
** Internal Header for Debugging of the Lua Implementation
** See Copyright Notice in lua.h
*/

#ifndef ltests_h
#define ltests_h


#include <stdlib.h>

/* do not use compatibility macros in Lua code */
#undef LUA_COMPAT_API

#define LUA_DEBUG

#undef NDEBUG
#include <assert.h>
#define lua_assert(c)           assert(c)


/* to avoid warnings, and to make sure value is really unused */
#define UNUSED(x)       (x=0, (void)(x))


/* memory allocator control variables */
typedef struct Memcontrol {
  unsigned long numblocks;
  unsigned long total;
  unsigned long maxmem;
  unsigned long memlimit;
  unsigned long objcount[5];
} Memcontrol;

extern Memcontrol l_memcontrol;


/*
** generic variable for debug tricks
*/
extern void *l_Trick;


void *debug_realloc (void *ud, void *block, size_t osize, size_t nsize);


typedef struct CallInfo *pCallInfo;

int lua_checkmemory (lua_State *L);


/* test for lock/unlock */
#undef luai_userstateopen
#undef luai_userstatethread
#undef lua_lock
#undef lua_unlock

struct L_EXTRA { int lock; int *plock; };
#define LUAI_EXTRASPACE		sizeof(struct L_EXTRA)
#define getlock(l)	(cast(struct L_EXTRA *, l) - 1)
#define luai_userstateopen(l)  \
	(getlock(l)->lock = 0, getlock(l)->plock = &(getlock(l)->lock))
#define luai_userstatethread(l,l1)  (getlock(l1)->plock = getlock(l)->plock)
#define lua_lock(l)     lua_assert((*getlock(l)->plock)++ == 0)
#define lua_unlock(l)   lua_assert(--(*getlock(l)->plock) == 0)


int luaB_opentests (lua_State *L);


#if defined(lua_c)
#define luaL_newstate()		lua_newstate(debug_realloc, &l_memcontrol)
#define luaL_openlibs(L)	{ (luaL_openlibs)(L); luaB_opentests(L); }
#endif



/* change some sizes to give some bugs a chance */

#undef LUAL_BUFFERSIZE
#define LUAL_BUFFERSIZE		23
#define MINSTRTABSIZE		2


#undef LUAI_USER_ALIGNMENT_T
#define LUAI_USER_ALIGNMENT_T   union { char b[32]; }


#endif
