/*
** $Id: $
** Interface to Memory Manager
** See Copyright Notice in lua.h
*/


#include <stdlib.h>

#include "lmem.h"
#include "lua.h"



int luaM_growaux (void **block, unsigned long nelems, int size,
                       char *errormsg, unsigned long limit)
{
  if (nelems >= limit)
    lua_error(errormsg);
  nelems = (nelems == 0) ? 32 : nelems*2;
  if (nelems > limit)
    nelems = limit;
  *block = luaM_realloc(*block, nelems*size);
  return (int)nelems;
}


static unsigned long Mbuffsize = 0;
static char *Mbuffer = NULL;


void *luaM_buffer (unsigned long size)
{
  if (size > Mbuffsize) {
    Mbuffsize = size;
    Mbuffer = luaM_realloc(Mbuffer, Mbuffsize);
  }
  return Mbuffer;
}


void luaM_clearbuffer (void)
{
  Mbuffsize /= 2;
  Mbuffer = luaM_realloc(Mbuffer, Mbuffsize);
}


#ifndef DEBUG

/*
** generic allocation routine.
** real ANSI systems do not need some of these tests,
** since realloc(NULL, s)==malloc(s) and realloc(b, 0)==free(b).
** But some systems (e.g. Sun OS) are not that ANSI...
*/
void *luaM_realloc (void *block, unsigned long size)
{
  size_t s = (size_t)size;
  if (s != size)
    lua_error("Allocation Error: Block too big");
  if (size == 0) {
    if (block) {
      free(block);
    }
    return NULL;
  }
  block = block ? realloc(block, s) : malloc(s);
  if (block == NULL)
    lua_error(memEM);
  return block;
}



#else
/* DEBUG */

#include <assert.h>
#include <string.h>


#define MARK    55

static unsigned long numblocks = 0;
static unsigned long totalmem = 0;



void luaM_query (void)
{
  lua_pushnumber(totalmem);
  lua_pushnumber(numblocks);
}


static void *checkblock (void *block)
{
  unsigned long *b = (unsigned long *)block - 1;
  unsigned long size = *b;
  assert(*(((char *)b)+size+sizeof(unsigned long)) == MARK);
  numblocks--;
  totalmem -= size;
  return b;
}


void *luaM_realloc (void *block, unsigned long size)
{
  unsigned long realsize = sizeof(unsigned long)+size+sizeof(char);
  if (realsize != (size_t)realsize)
    lua_error("Allocation Error: Block too big");
  if (size == 0) {  /* ANSI doen't need this, but some machines... */
    if (block) {
      memset(block, -1, *((unsigned long *)block-1));  /* erase block */
      block = checkblock(block);
      free(block);
    }
    return NULL;
  }
  if (block) {
    block = checkblock(block);
    block = (unsigned long *)realloc(block, realsize);
  }
  else
    block = (unsigned long *)malloc(realsize);
  if (block == NULL)
    lua_error(memEM);
  totalmem += size;
  numblocks++;
  *(unsigned long *)block = size;
  *(((char *)block)+size+sizeof(unsigned long)) = MARK;
  return (unsigned long *)block+1;
}


#endif
