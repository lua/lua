/*
** $Id: ltests.h,v 2.54 2017/12/07 18:51:39 roberto Exp roberto $
** Internal Header for Debugging of the Lua Implementation
** See Copyright Notice in lua.h
*/

#ifndef ltests_h
#define ltests_h


#include <stdio.h>
#include <stdlib.h>

/* test Lua with compatibility code */
#define LUA_COMPAT_MATHLIB
#define LUA_COMPAT_IPAIRS
#define LUA_COMPAT_BITLIB
#define LUA_COMPAT_APIINTCASTS
#define LUA_COMPAT_FLOATSTRING
#define LUA_COMPAT_UNPACK
#define LUA_COMPAT_LOADERS
#define LUA_COMPAT_LOG10
#define LUA_COMPAT_LOADSTRING
#define LUA_COMPAT_MAXN
#define LUA_COMPAT_MODULE


#define LUA_DEBUG


/* turn on assertions */
#undef NDEBUG
#include <assert.h>
#define lua_assert(c)           assert(c)


/* compiled with -O0, Lua uses a lot of C stack space... */
#undef LUAI_MAXCCALLS
#define LUAI_MAXCCALLS	200

/* to avoid warnings, and to make sure value is really unused */
#define UNUSED(x)       (x=0, (void)(x))


/* test for sizes in 'l_sprintf' (make sure whole buffer is available) */
#undef l_sprintf
#if !defined(LUA_USE_C89)
#define l_sprintf(s,sz,f,i)	(memset(s,0xAB,sz), snprintf(s,sz,f,i))
#else
#define l_sprintf(s,sz,f,i)	(memset(s,0xAB,sz), sprintf(s,f,i))
#endif


/* memory-allocator control variables */
typedef struct Memcontrol {
  unsigned long numblocks;
  unsigned long total;
  unsigned long maxmem;
  unsigned long memlimit;
  unsigned long countlimit;
  unsigned long objcount[LUA_NUMTAGS];
} Memcontrol;

LUA_API Memcontrol l_memcontrol;


/*
** generic variable for debug tricks
*/
extern void *l_Trick;



/*
** Function to traverse and check all memory used by Lua
*/
int lua_checkmemory (lua_State *L);


/* test for lock/unlock */

struct L_EXTRA { int lock; int *plock; };
#undef LUA_EXTRASPACE
#define LUA_EXTRASPACE	sizeof(struct L_EXTRA)
#define getlock(l)	cast(struct L_EXTRA*, lua_getextraspace(l))
#define luai_userstateopen(l)  \
	(getlock(l)->lock = 0, getlock(l)->plock = &(getlock(l)->lock))
#define luai_userstateclose(l)  \
  lua_assert(getlock(l)->lock == 1 && getlock(l)->plock == &(getlock(l)->lock))
#define luai_userstatethread(l,l1) \
  lua_assert(getlock(l1)->plock == getlock(l)->plock)
#define luai_userstatefree(l,l1) \
  lua_assert(getlock(l)->plock == getlock(l1)->plock)
#define lua_lock(l)     lua_assert((*getlock(l)->plock)++ == 0)
#define lua_unlock(l)   lua_assert(--(*getlock(l)->plock) == 0)



LUA_API int luaB_opentests (lua_State *L);

LUA_API void *debug_realloc (void *ud, void *block,
                             size_t osize, size_t nsize);

#if defined(lua_c)
#define luaL_newstate()		lua_newstate(debug_realloc, &l_memcontrol)
#define luaL_openlibs(L)  \
  { (luaL_openlibs)(L); \
     luaL_requiref(L, "T", luaB_opentests, 1); \
     lua_pop(L, 1); }
#endif



/* change some sizes to give some bugs a chance */

#undef LUAL_BUFFERSIZE
#define LUAL_BUFFERSIZE		23
#define MINSTRTABSIZE		2
#define MAXINDEXRK		1
#define MAXIWTHABS		3


/* make stack-overflow tests run faster */
#undef LUAI_MAXSTACK
#define LUAI_MAXSTACK   50000


#undef LUAI_USER_ALIGNMENT_T
#define LUAI_USER_ALIGNMENT_T   union { char b[sizeof(void*) * 8]; }


#define STRCACHE_N	23
#define STRCACHE_M	5

#endif

