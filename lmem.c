/*
** $Id: lmem.c,v 1.12 1999/02/25 21:07:26 roberto Exp roberto $
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


#define MINSIZE	16	/* minimum size for "growing" vectors */



#ifndef DEBUG


static unsigned long power2 (unsigned long n) {
  unsigned long p = MINSIZE;
  while (p<=n) p<<=1;
  return p;
}


void *luaM_growaux (void *block, unsigned long nelems, int inc, int size,
                       char *errormsg, unsigned long limit) {
  unsigned long newn = nelems+inc;
  if ((newn ^ nelems) > nelems) {  /* cross a power of 2 boundary? */
    if (newn >= limit)
      lua_error(errormsg);
    newn = power2(newn);
    if (newn > limit)
      newn = limit;
    return luaM_realloc(block, newn*size);
  }
  else
    return block;
}


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


void *luaM_growaux (void *block, unsigned long nelems, int inc, int size,
                       char *errormsg, unsigned long limit) {
  unsigned long newn = nelems+inc;
  if (newn >= limit)
    lua_error(errormsg);
  return luaM_realloc(block, newn*size);
}


#define HEADER	(sizeof(double))

#define MARK    55

unsigned long numblocks = 0;
unsigned long totalmem = 0;


static void *checkblock (void *block) {
  unsigned long *b = (unsigned long *)((char *)block - HEADER);
  unsigned long size = *b;
  LUA_ASSERT(*(((char *)b)+size+HEADER) == MARK, 
             "corrupted block");
  numblocks--;
  totalmem -= size;
  return b;
}


void *luaM_realloc (void *block, unsigned long size) {
  unsigned long realsize = HEADER+size+1;
  if (realsize != (size_t)realsize)
    lua_error("memory allocation error: block too big");
  if (size == 0) {
    if (block) {
      unsigned long *b = (unsigned long *)((char *)block - HEADER);
      memset(block, -1, *b);  /* erase block */
      block = checkblock(block);
    }
    free(block);
    return NULL;
  }
  if (block)
    block = checkblock(block);
  block = (unsigned long *)realloc(block, realsize);
  if (block == NULL)
    lua_error(memEM);
  totalmem += size;
  numblocks++;
  *(unsigned long *)block = size;
  *(((char *)block)+size+HEADER) = MARK;
  return (unsigned long *)((char *)block+HEADER);
}


#endif
