/*
** mem.c
** TecCGraf - PUC-Rio
*/

char *rcs_mem = "$Id: mem.c,v 1.1 1994/11/16 17:38:08 roberto Exp $";

#include <stdlib.h>

#include "mem.h"
#include "lua.h"

void luaI_free (void *block)
{
  *((int *)block) = -1;  /* to catch errors */
  free(block);
}


void *luaI_malloc (unsigned long size)
{
  void *block = malloc(size);
  if (block == NULL)
    lua_error("not enough memory");
  return block;
}


void *luaI_realloc (void *oldblock, unsigned long size)
{
  void *block = realloc(oldblock, size);
  if (block == NULL)
    lua_error("not enough memory");
  return block;
}

