/*
** mem.c
** TecCGraf - PUC-Rio
*/

char *rcs_mem = "$Id: mem.c,v 1.12 1996/05/06 16:59:00 roberto Exp $";

#include <stdlib.h>

#include "mem.h"
#include "lua.h"


#define mem_error()  lua_error(memEM)
 

void luaI_free (void *block)
{
  if (block)
  {
    *((int *)block) = -1;  /* to catch errors */
    free(block);
  }
}


void *luaI_malloc (unsigned long size)
{
  void *block = malloc((size_t)size);
  if (block == NULL)
    mem_error();
  return block;
}


void *luaI_realloc (void *oldblock, unsigned long size)
{
  void *block = oldblock ? realloc(oldblock, (size_t)size) :
                           malloc((size_t)size);
  if (block == NULL)
    mem_error();
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
  return (int) nelems;
}


void* luaI_buffer (unsigned long size)
{
  static unsigned long buffsize = 0;
  static char* buffer = NULL;
  if (size > buffsize)
    buffer = luaI_realloc(buffer, buffsize=size);
  return buffer;
}

