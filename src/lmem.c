/*
** $Id: lmem.c,v 1.17 1999/05/24 17:51:05 roberto Exp $
** Interface to Memory Manager
** See Copyright Notice in lua.h
*/


#include <stdlib.h>

#include "lmem.h"
#include "lstate.h"
#include "lua.h"


/*
** real ANSI systems do not need these tests;
** but some systems (Sun OS) are not that ANSI...
*/
#ifdef OLD_ANSI
#define realloc(b,s)	((b) == NULL ? malloc(s) : (realloc)(b, s))
#define free(b)		if (b) (free)(b)
#endif


#define MINSIZE	8	/* minimum size for "growing" vectors */




static unsigned long power2 (unsigned long n) {
  unsigned long p = MINSIZE;
  while (p<=n) p<<=1;
  return p;
}


void *luaM_growaux (void *block, unsigned long nelems, int inc, int size,
                       char *errormsg, unsigned long limit) {
  unsigned long newn = nelems+inc;
  if (newn >= limit) lua_error(errormsg);
  if ((newn ^ nelems) <= nelems ||  /* still the same power of 2 limit? */
       (nelems > 0 && newn < MINSIZE))  /* or block already is MINSIZE? */
      return block;  /* do not need to reallocate */
  else  /* it crossed a power of 2 boundary; grow to next power */
    return luaM_realloc(block, power2(newn)*size);
}


#ifndef DEBUG

/*
** generic allocation routine.
*/
void *luaM_realloc (void *block, unsigned long size) {
  size_t s = (size_t)size;
  if (s != size)
    lua_error("memory allocation error: block too big");
  if (size == 0) {
    free(block);  /* block may be NULL, that is OK for free */
    return NULL;
  }
  block = realloc(block, s);
  if (block == NULL)
    lua_error(memEM);
  return block;
}



#else
/* DEBUG */

#include <string.h>


#define HEADER	(sizeof(double))
#define MARKSIZE	16

#define MARK    55


#define blocksize(b)	((unsigned long *)((char *)(b) - HEADER))

unsigned long numblocks = 0;
unsigned long totalmem = 0;


static void *checkblock (void *block) {
  if (block == NULL)
    return NULL;
  else {
    unsigned long *b = blocksize(block);
    unsigned long size = *b;
    int i;
    for (i=0;i<MARKSIZE;i++)
      LUA_ASSERT(*(((char *)b)+HEADER+size+i) == MARK+i, "corrupted block");
    numblocks--;
    totalmem -= size;
    return b;
  }
}


static void freeblock (void *block) {
  if (block)
    memset(block, -1, *blocksize(block));  /* erase block */
  free(checkblock(block));
}


void *luaM_realloc (void *block, unsigned long size) {
  unsigned long realsize = HEADER+size+MARKSIZE;
  if (realsize != (size_t)realsize)
    lua_error("memory allocation error: block too big");
  if (size == 0) {
    freeblock(block);
    return NULL;
  }
  else {
    char *newblock = malloc(realsize);
    int i;
    if (block) {
      unsigned long oldsize = *blocksize(block);
      if (oldsize > size) oldsize = size;
      memcpy(newblock+HEADER, block, oldsize);
      freeblock(block);  /* erase (and check) old copy */
    }
    if (newblock == NULL)
      lua_error(memEM);
    totalmem += size;
    numblocks++;
    *(unsigned long *)newblock = size;
    for (i=0;i<MARKSIZE;i++)
      *(newblock+HEADER+size+i) = MARK+i;
    return newblock+HEADER;
  }
}


#endif
