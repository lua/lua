/*
** $Id: lmem.c,v 1.42 2000/12/28 12:55:41 roberto Exp roberto $
** Interface to Memory Manager
** See Copyright Notice in lua.h
*/


#include <stdlib.h>

#include "lua.h"

#include "ldo.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"




#ifdef LUA_DEBUG
/*
** {======================================================================
** Controlled version for realloc.
** =======================================================================
*/


#include <assert.h>
#include <limits.h>
#include <string.h>

#define basicrealloc(b, os, s)	debug_realloc(b, os, s)
#define basicfree(b, s)		debug_realloc(b, s, 0)


/* ensures maximum alignment for HEADER */
#define HEADER	(sizeof(union L_Umaxalign))

#define MARKSIZE	32
#define MARK		0x55  /* 01010101 (a nice pattern) */


#define blocksize(b)	((size_t *)((char *)(b) - HEADER))

mem_int memdebug_numblocks = 0;
mem_int memdebug_total = 0;
mem_int memdebug_maxmem = 0;
mem_int memdebug_memlimit = LONG_MAX;


static void *checkblock (void *block) {
  size_t *b = blocksize(block);
  size_t size = *b;
  int i;
  for (i=0;i<MARKSIZE;i++)
    assert(*(((char *)b)+HEADER+size+i) == MARK+i);  /* corrupted block? */
  return b;
}


static void freeblock (void *block) {
  if (block) {
    size_t size = *blocksize(block);
    block = checkblock(block);
    memset(block, -1, size+HEADER+MARKSIZE);  /* erase block */
    free(block);  /* free original block */
    memdebug_numblocks--;
    memdebug_total -= size;
  }
}


static void *debug_realloc (void *block, size_t oldsize, size_t size) {
  assert((oldsize == 0) ? block == NULL : oldsize == *blocksize(block));
  if (size == 0) {
    freeblock(block);
    return NULL;
  }
  else if (memdebug_total+size > memdebug_memlimit)
    return NULL;  /* to test memory allocation errors */
  else {
    char *newblock;
    int i;
    size_t realsize = HEADER+size+MARKSIZE;
    if (realsize < size) return NULL;  /* overflow! */
    newblock = (char *)malloc(realsize);  /* alloc a new block */
    if (newblock == NULL) return NULL;
    if (oldsize > size) oldsize = size;
    if (block) {
      memcpy(newblock+HEADER, block, oldsize);
      freeblock(block);  /* erase (and check) old copy */
    }
    /* initialize new part of the block with something `weird' */
    memset(newblock+HEADER+oldsize, -MARK, size-oldsize);
    memdebug_total += size;
    if (memdebug_total > memdebug_maxmem)
      memdebug_maxmem = memdebug_total;
    memdebug_numblocks++;
    *(size_t *)newblock = size;
    for (i=0;i<MARKSIZE;i++)
      *(newblock+HEADER+size+i) = (char)(MARK+i);
    return newblock+HEADER;
  }
}


/* }====================================================================== */

#else
/* no debug */

/*
** Real ISO (ANSI) systems do not need these tests;
** but some systems (Sun OS) are not that ISO...
*/
#ifdef OLD_ANSI
#define basicrealloc(b,os,s)	((b) == NULL ? malloc(s) : realloc(b, s))
#define basicfree(b,s)		if (b) free(b)
#else
#define basicrealloc(b,os,s)	realloc(b,s)
#define basicfree(b,s)		free(b)

#endif


#endif

void *luaM_growaux (lua_State *L, void *block, int *size, int size_elems,
                    int limit, const char *errormsg) {
  void *newblock;
  int newsize = (*size)*2;
  if (newsize < MINPOWER2)
    newsize = MINPOWER2;  /* minimum size */
  else if (*size >= limit/2) {  /* cannot double it? */
    if (*size < limit - MINPOWER2)  /* try something smaller... */
      newsize = limit;  /* still have at least MINPOWER2 free places */
    else lua_error(L, errormsg);
  }
  newblock = luaM_realloc(L, block, (luint32)(*size)*(luint32)size_elems,
                                    (luint32)newsize*(luint32)size_elems);
  *size = newsize;  /* update only when everything else is OK */
  return newblock;
}


/*
** generic allocation routine.
*/
void *luaM_realloc (lua_State *L, void *block, luint32 oldsize, luint32 size) {
  if (size == 0) {
    basicfree(block, oldsize);  /* block may be NULL; that is OK for free */
    block = NULL;
  }
  else if (size >= MAX_SIZET)
    lua_error(L, "memory allocation error: block too big");
  else {
    block = basicrealloc(block, oldsize, size);
    if (block == NULL) {
      if (L)
        luaD_breakrun(L, LUA_ERRMEM);  /* break run without error message */
      else return NULL;  /* error before creating state! */
    }
  }
  if (L && G(L)) {
    G(L)->nblocks -= oldsize;
    G(L)->nblocks += size;
  }
  return block;
}

