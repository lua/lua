/*
** mem.c
** TecCGraf - PUC-Rio
*/

char *rcs_luamem = "$Id: luamem.c,v 1.16 1997/04/01 21:23:20 roberto Exp $";

#include <stdlib.h>

#include "luamem.h"
#include "lua.h"


#define DEBUG 0

#if !DEBUG

void luaI_free (void *block)
{
  if (block)
  {
    *((char *)block) = -1;  /* to catch errors */
    free(block);
  }
}


void *luaI_realloc (void *oldblock, unsigned long size)
{
  void *block;
  size_t s = (size_t)size;
  if (s != size)
    lua_error("Allocation Error: Block too big");
  block = oldblock ? realloc(oldblock, s) : malloc(s);
  if (block == NULL)
    lua_error(memEM);
  return block;
}


int luaI_growvector (void **block, unsigned long nelems, int size,
                       char *errormsg, unsigned long limit)
{
  if (nelems >= limit)
    lua_error(errormsg);
  nelems = (nelems == 0) ? 20 : nelems*2;
  if (nelems > limit)
    nelems = limit;
  *block = luaI_realloc(*block, nelems*size);
  return (int)nelems;
}


void* luaI_buffer (unsigned long size)
{
  static unsigned long buffsize = 0;
  static char* buffer = NULL;
  if (size > buffsize)
    buffer = luaI_realloc(buffer, buffsize=size);
  return buffer;
}

#else
/* DEBUG */

#include <stdio.h>

# define assert(ex)    {if (!(ex)){(void)fprintf(stderr, \
    "Assertion failed: file \"%s\", line %d\n", __FILE__, __LINE__);exit(1);}}

#define MARK    55

static unsigned long numblocks = 0;
static unsigned long totalmem = 0;


static void message (void)
{
#define inrange(x,y)   ((x) < (((y)*3)/2)  && (x) > (((y)*2)/3))
  static int count = 0;
  static unsigned long lastnumblocks = 0;
  static unsigned long lasttotalmem = 0;
  if (!inrange(numblocks, lastnumblocks) || !inrange(totalmem, lasttotalmem)
       || count++ >= 5000)
  {
    fprintf(stderr,"blocks = %lu  mem = %luK\n", numblocks, totalmem/1000);
    count = 0;
    lastnumblocks = numblocks;
    lasttotalmem = totalmem;
  }
}


void luaI_free (void *block)
{
  if (block)
  {
    unsigned long *b = (unsigned long *)block - 1;
    unsigned long size = *b;
    assert(*(((char *)b)+size+sizeof(unsigned long)) == MARK);
    numblocks--;
    totalmem -= size;
    free(b);
    message();
  }
}


void *luaI_realloc (void *oldblock, unsigned long size)
{
  unsigned long *block;
  unsigned long realsize = sizeof(unsigned long)+size+sizeof(char);
  if (realsize != (size_t)realsize)
    lua_error("Allocation Error: Block too big");
  if (oldblock)
  {
    unsigned long *b = (unsigned long *)oldblock - 1;
    unsigned long oldsize = *b;
    assert(*(((char *)b)+oldsize+sizeof(unsigned long)) == MARK);
    totalmem -= oldsize;
    numblocks--;
    block = (unsigned long *)realloc(b, realsize);
  }
  else
    block = (unsigned long *)malloc(realsize);
  if (block == NULL)
    lua_error("not enough memory");
  totalmem += size;
  numblocks++;
  *block = size;
  *(((char *)block)+size+sizeof(unsigned long)) = MARK;
  message();
  return block+1;
}


int luaI_growvector (void **block, unsigned long nelems, int size,
                       char *errormsg, unsigned long limit)
{
  if (nelems >= limit)
    lua_error(errormsg);
  nelems = (nelems == 0) ? 20 : nelems*2;
  if (nelems > limit)
    nelems = limit;
  *block = luaI_realloc(*block, nelems*size);
  return (int)nelems;
}


void* luaI_buffer (unsigned long size)
{
  static unsigned long buffsize = 0;
  static char* buffer = NULL;
  if (size > buffsize)
    buffer = luaI_realloc(buffer, buffsize=size);
  return buffer;
}

#endif
