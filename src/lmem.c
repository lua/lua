/*
** $Id: lmem.c,v 1.65 2004/08/30 13:44:44 roberto Exp $
** Interface to Memory Manager
** See Copyright Notice in lua.h
*/


#include <stddef.h>

#define lmem_c
#define LUA_CORE

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"



/*
** About the realloc function:
** void * realloc (void *ud, void *ptr, size_t osize, size_t nsize);
** (`osize' is the old size, `nsize' is the new size)
**
** Lua ensures that (ptr == NULL) iff (osize == 0).
**
** * realloc(ud, NULL, 0, x) creates a new block of size `x'
**
** * realloc(ud, p, x, 0) frees the block `p'
** (in this specific case, realloc must return NULL).
** particularly, realloc(ud, NULL, 0, 0) does nothing
** (which is equivalent to free(NULL) in ANSI C)
**
** realloc returns NULL if it cannot create or reallocate the area
** (any reallocation to an equal or smaller size cannot fail!)
*/



#define MINSIZEARRAY	4


void *luaM_growaux (lua_State *L, void *block, int *size, int size_elems,
                    int limit, const char *errormsg) {
  void *newblock;
  int newsize;
  if (*size >= limit/2) {  /* cannot double it? */
    if (*size >= limit - MINSIZEARRAY)  /* try something smaller... */
      luaG_runerror(L, errormsg);
    newsize = limit;  /* still have at least MINSIZEARRAY free places */
  }
  else {
    newsize = (*size)*2;
    if (newsize < MINSIZEARRAY)
      newsize = MINSIZEARRAY;  /* minimum size */
  }
  newblock = luaM_realloc(L, block,
                          cast(lu_mem, *size)*cast(lu_mem, size_elems),
                          cast(lu_mem, newsize)*cast(lu_mem, size_elems));
  *size = newsize;  /* update only when everything else is OK */
  return newblock;
}


/*
** generic allocation routine.
*/
void *luaM_realloc (lua_State *L, void *block, lu_mem osize, lu_mem nsize) {
  global_State *g = G(L);
  lua_assert((osize == 0) == (block == NULL));
  if (nsize >= MAX_SIZET)
    luaG_runerror(L, "memory allocation error: block too big");
  block = (*g->realloc)(g->ud, block, osize, nsize);
  if (block == NULL && nsize > 0)
    luaD_throw(L, LUA_ERRMEM);
  lua_assert((nsize == 0) == (block == NULL));
  g->totalbytes = (g->totalbytes - osize) + nsize;
  return block;
}

