/*
** mem.c
** TecCGraf - PUC-Rio
*/

char *rcs_mem = "$Id: mem.c,v 1.5 1995/02/06 19:34:03 roberto Exp roberto $";

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "mem.h"
#include "lua.h"
#include "table.h"

static void mem_error (void)
{
  Long recovered = luaI_collectgarbage();  /* try to collect garbage  */
  if (recovered)
    lua_error("not enough memory");
  else
  { /* if there is no garbage then must exit */
    printf(stderr, "lua error: memory overflow - unable to recover\n");
    exit(1);
  }
}

void luaI_free (void *block)
{
  *((int *)block) = -1;  /* to catch errors */
  free(block);
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
  void *block = realloc(oldblock, (size_t)size);
  if (block == NULL)
    mem_error();
  return block;
}


char *luaI_strdup (char *str)
{
  char *newstr = luaI_malloc(strlen(str)+1);
  strcpy(newstr, str);
  return newstr;
}
