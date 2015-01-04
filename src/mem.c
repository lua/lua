/*
** mem.c
** TecCGraf - PUC-Rio
*/

char *rcs_mem = "$Id: mem.c,v 1.5 1995/02/06 19:34:03 roberto Exp $";

#include <stdlib.h>
#include <string.h>

#include "mem.h"
#include "lua.h"

void luaI_free (void *block)
{
  *((int *)block) = -1;  /* to catch errors */
  free(block);
}


void *luaI_malloc (unsigned long size)
{
  void *block = malloc((size_t)size);
  if (block == NULL)
    lua_error("not enough memory");
  return block;
}


void *luaI_realloc (void *oldblock, unsigned long size)
{
  void *block = realloc(oldblock, (size_t)size);
  if (block == NULL)
    lua_error("not enough memory");
  return block;
}


char *luaI_strdup (char *str)
{
  char *newstr = luaI_malloc(strlen(str)+1);
  strcpy(newstr, str);
  return newstr;
}
