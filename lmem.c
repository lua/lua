/*
** $Id: lmem.c,v 1.55 2002/05/15 18:57:44 roberto Exp roberto $
** Interface to Memory Manager
** See Copyright Notice in lua.h
*/


#include <stdlib.h>

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"



/*
** definition for realloc function. It must assure that
** l_realloc(block, x, 0) frees the block, and l_realloc(NULL, 0, x)
** allocates a new block (ANSI C assures that).
** (`os' is the old block size; some allocators may use that.)
*/
#ifndef l_realloc
#define l_realloc(b,os,s)	realloc(b,s)
#endif


#define MINSIZEARRAY	4


void *luaM_growaux (lua_State *L, void *block, int *size, int size_elems,
                    int limit, const char *errormsg) {
  void *newblock;
  int newsize = (*size)*2;
  if (newsize < MINSIZEARRAY)
    newsize = MINSIZEARRAY;  /* minimum size */
  else if (*size >= limit/2) {  /* cannot double it? */
    if (*size < limit - MINSIZEARRAY)  /* try something smaller... */
      newsize = limit;  /* still have at least MINSIZEARRAY free places */
    else luaG_runerror(L, errormsg);
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
void *luaM_realloc (lua_State *L, void *block, lu_mem oldsize, lu_mem size) {
  if (size == 0) {
    if (block != NULL) {
      l_realloc(block, oldsize, size);
      block = NULL;
    }
  }
  else if (size >= MAX_SIZET)
    luaG_runerror(L, "memory allocation error: block too big");
  else {
    block = l_realloc(block, oldsize, size);
    if (block == NULL) {
      if (L)
        luaD_error(L, MEMERRMSG, LUA_ERRMEM);
      else return NULL;  /* error before creating state! */
    }
  }
  if (L && G(L)) {
    G(L)->nblocks -= oldsize;
    G(L)->nblocks += size;
  }
  return block;
}

