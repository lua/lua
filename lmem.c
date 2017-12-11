/*
** $Id: lmem.c,v 1.94 2017/12/08 17:28:25 roberto Exp roberto $
** Interface to Memory Manager
** See Copyright Notice in lua.h
*/

#define lmem_c
#define LUA_CORE

#include "lprefix.h"


#include <stddef.h>

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"


#if defined(HARDMEMTESTS)
#define hardtest(L,os,s)  /* force a GC whenever possible */ \
  if ((s) > (os) && (G(L))->gcrunning) luaC_fullgc(L, 1);
#else
#define hardtest(L,os,s)  ((void)0)
#endif



/*
** About the realloc function:
** void * frealloc (void *ud, void *ptr, size_t osize, size_t nsize);
** ('osize' is the old size, 'nsize' is the new size)
**
** * frealloc(ud, NULL, x, s) creates a new block of size 's' (no
** matter 'x').
**
** * frealloc(ud, p, x, 0) frees the block 'p'
** (in this specific case, frealloc must return NULL);
** particularly, frealloc(ud, NULL, 0, 0) does nothing
** (which is equivalent to free(NULL) in ISO C)
**
** frealloc returns NULL if it cannot create or reallocate the area
** (any reallocation to an equal or smaller size cannot fail!)
*/



#define MINSIZEARRAY	4


void *luaM_growaux_ (lua_State *L, void *block, int nelems, int *psize,
                     int size_elems, int limit, const char *what) {
  void *newblock;
  int size = *psize;
  if (nelems + 1 <= size)  /* does one extra element still fit? */
    return block;  /* nothing to be done */
  if (size >= limit / 2) {  /* cannot double it? */
    if (size >= limit)  /* cannot grow even a little? */
      luaG_runerror(L, "too many %s (limit is %d)", what, limit);
    size = limit;  /* still have at least one free place */
  }
  else {
    size *= 2;
    if (size < MINSIZEARRAY)
      size = MINSIZEARRAY;  /* minimum size */
  }
  lua_assert(nelems + 1 <= size && size <= limit);
  /* 'limit' ensures that multiplication will not overflow */
  newblock = luaM_realloc_(L, block, cast(size_t, *psize) * size_elems,
                                     cast(size_t, size) * size_elems);
  if (newblock == NULL)
    luaM_error(L);
  *psize = size;  /* update only when everything else is OK */
  return newblock;
}


void *luaM_shrinkvector_ (lua_State *L, void *block, int *size,
                          int final_n, int size_elem) {
  global_State *g = G(L);
  void *newblock;
  size_t oldsize = cast(size_t, (*size) * size_elem);
  size_t newsize = cast(size_t, final_n * size_elem);
  lua_assert(newsize <= oldsize);
  newblock = (*g->frealloc)(g->ud, block, oldsize, newsize);
  if (newblock == NULL && final_n > 0)  /* allocation failed? */
    luaM_error(L);
  else {
    g->GCdebt += newsize - oldsize;
    *size = final_n;
    return newblock;
  }
}


l_noret luaM_toobig (lua_State *L) {
  luaG_runerror(L, "memory allocation error: block too big");
}


/*
** Free memory
*/
void luaM_free_ (lua_State *L, void *block, size_t osize) {
  global_State *g = G(L);
  lua_assert((block == 0) == (block == NULL));
  (*g->frealloc)(g->ud, block, osize, 0);
  g->GCdebt -= osize;
}



/*
** generic allocation routine.
*/
void *luaM_realloc_ (lua_State *L, void *block, size_t osize, size_t nsize) {
  void *newblock;
  global_State *g = G(L);
  lua_assert((osize == 0) == (block == NULL));
  hardtest(L, osize, nsize);
  newblock = (*g->frealloc)(g->ud, block, osize, nsize);
  if (newblock == NULL && nsize > 0) {
    /* Is state fully built? Not shrinking a block? */
    if (g->version && nsize > osize) {
      luaC_fullgc(L, 1);  /* try to free some memory... */
      newblock = (*g->frealloc)(g->ud, block, osize, nsize);  /* try again */
    }
    if (newblock == NULL)
      return NULL;
  }
  lua_assert((nsize == 0) == (newblock == NULL));
  g->GCdebt = (g->GCdebt + nsize) - osize;
  return newblock;
}


void *luaM_saferealloc_ (lua_State *L, void *block, size_t osize,
                                                    size_t nsize) {
  void *newblock = luaM_realloc_(L, block, osize, nsize);
  if (newblock == NULL && nsize > 0)  /* allocation failed? */
    luaM_error(L);
  return newblock;
}


void *luaM_malloc_ (lua_State *L, size_t size, int tag) {
  hardtest(L, 0, size);
  if (size == 0)
    return NULL;  /* that's all */
  else {
    global_State *g = G(L);
    void *newblock = (*g->frealloc)(g->ud, NULL, tag, size);
    if (newblock == NULL) {
      if (g->version) {  /* is state fully built? */
        luaC_fullgc(L, 1);  /* try to free some memory... */
        newblock = (*g->frealloc)(g->ud, NULL, tag, size);  /* try again */
      }
      if (newblock == NULL)
        luaM_error(L);
    }
    g->GCdebt += size;
    return newblock;
  }
}
