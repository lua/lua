/*
** $Id: lmem.c,v 1.7 1998/06/29 22:03:06 roberto Exp $
** Interface to Memory Manager
** See Copyright Notice in lua.h
*/


#include <stdlib.h>

#include "lmem.h"
#include "lstate.h"
#include "lua.h"


/*
** real ANSI systems do not need some of these tests,
** since realloc(NULL, s)==malloc(s).
** But some systems (Sun OS) are not that ANSI...
*/
#ifdef OLD_ANSI
#define realloc(b,s)	((b) == NULL ? malloc(s) : (realloc)(b, s))
#endif



int luaM_growaux (void **block, unsigned long nelems, int size,
                       char *errormsg, unsigned long limit) {
  if (nelems >= limit)
    lua_error(errormsg);
  nelems = (nelems == 0) ? 32 : nelems*2;
  if (nelems > limit)
    nelems = limit;
  *block = luaM_realloc(*block, nelems*size);
  return (int)nelems;
}



#ifndef DEBUG

/*
** generic allocation routine.
*/
void *luaM_realloc (void *block, unsigned long size) {
  size_t s = (size_t)size;
  if (s != size)
    lua_error("Allocation Error: Block too big");
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
    lua_error("Allocation Error: Block too big");
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
