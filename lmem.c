/*
** $Id: lmem.c,v 1.28 2000/03/10 18:37:44 roberto Exp roberto $
** Interface to Memory Manager
** See Copyright Notice in lua.h
*/


#include <stdlib.h>

#define LUA_REENTRANT

#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lua.h"


/*
** Number ANSI systems do not need these tests;
** but some systems (Sun OS) are not that ANSI...
*/
#ifdef OLD_ANSI
#define realloc(b,s)	((b) == NULL ? malloc(s) : (realloc)(b, s))
#define free(b)		if (b) (free)(b)
#endif



#ifdef DEBUG
/*
** {======================================================================
** Controlled version for realloc.
** =======================================================================
*/


#include <assert.h>
#include <string.h>


#define realloc(b, s)	debug_realloc(b, s)
#define malloc(b)	debug_realloc(NULL, 0)
#define free(b)		debug_realloc(b, 0)


/* ensures maximum alignment for HEADER */
#define HEADER	(sizeof(double)>sizeof(long) ? sizeof(double) : sizeof(long))

#define MARKSIZE	16
#define MARK		0x55  /* 01010101 (a nice pattern) */


#define blocksize(b)	((unsigned long *)((char *)(b) - HEADER))

unsigned long memdebug_numblocks = 0;
unsigned long memdebug_total = 0;
unsigned long memdebug_maxmem = 0;


static void *checkblock (void *block) {
  unsigned long *b = blocksize(block);
  unsigned long size = *b;
  int i;
  for (i=0;i<MARKSIZE;i++)
    assert(*(((char *)b)+HEADER+size+i) == MARK+i);  /* corrupted block? */
  memdebug_numblocks--;
  memdebug_total -= size;
  return b;
}


static void freeblock (void *block) {
  if (block) {
    size_t size = *blocksize(block);
    block = checkblock(block);
    memset(block, -1, size+HEADER+MARKSIZE);  /* erase block */
    (free)(block);  /* free original block */
  }
}


static void *debug_realloc (void *block, size_t size) {
  if (size == 0) {
    freeblock(block);
    return NULL;
  }
  else {
    size_t realsize = HEADER+size+MARKSIZE;
    char *newblock = (char *)(malloc)(realsize);  /* alloc a new block */
    int i;
    if (newblock == NULL) return NULL;
    if (block) {
      size_t oldsize = *blocksize(block);
      if (oldsize > size) oldsize = size;
      memcpy(newblock+HEADER, block, oldsize);
      freeblock(block);  /* erase (and check) old copy */
    }
    memdebug_total += size;
    if (memdebug_total > memdebug_maxmem) memdebug_maxmem = memdebug_total;
    memdebug_numblocks++;
    *(unsigned long *)newblock = size;
    for (i=0;i<MARKSIZE;i++)
      *(newblock+HEADER+size+i) = (char)(MARK+i);
    return newblock+HEADER;
  }
}


/* }====================================================================== */
#endif



void *luaM_growaux (lua_State *L, void *block, unsigned long nelems,
               int inc, int size, const char *errormsg, unsigned long limit) {
  unsigned long newn = nelems+inc;
  if (newn >= limit) lua_error(L, errormsg);
  if ((newn ^ nelems) <= nelems ||  /* still the same power-of-2 limit? */
       (nelems > 0 && newn < MINPOWER2))  /* or block already is MINPOWER2? */
      return block;  /* do not need to reallocate */
  else  /* it crossed a power-of-2 boundary; grow to next power */
    return luaM_realloc(L, block, luaO_power2(newn)*size);
}


/*
** generic allocation routine.
*/
void *luaM_realloc (lua_State *L, void *block, unsigned long size) {
  if (size == 0) {
    free(block);  /* block may be NULL; that is OK for free */
    return NULL;
  }
  else if ((size_t)size != size)
    lua_error(L, "memory allocation error: block too big");
  block = realloc(block, size);
  if (block == NULL)
    lua_error(L, memEM);
  return block;
}


