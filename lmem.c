/*
** $Id: lmem.c,v 1.47 2001/02/20 18:15:33 roberto Exp roberto $
** Interface to Memory Manager
** See Copyright Notice in lua.h
*/


#include <stdlib.h>

#include "lua.h"

#include "ldo.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"



#ifndef l_realloc
#define l_realloc(b,os,s)	realloc(b,s)
#define l_free(b,s)		free(b)
#endif


void *luaM_growaux (lua_State *L, void *block, int *size, int size_elems,
                    int limit, const l_char *errormsg) {
  void *newblock;
  int newsize = (*size)*2;
  if (newsize < MINPOWER2)
    newsize = MINPOWER2;  /* minimum size */
  else if (*size >= limit/2) {  /* cannot double it? */
    if (*size < limit - MINPOWER2)  /* try something smaller... */
      newsize = limit;  /* still have at least MINPOWER2 free places */
    else luaD_error(L, errormsg);
  }
  newblock = luaM_realloc(L, block, (lu_mem)(*size)*(lu_mem)size_elems,
                                    (lu_mem)newsize*(lu_mem)size_elems);
  *size = newsize;  /* update only when everything else is OK */
  return newblock;
}


/*
** generic allocation routine.
*/
void *luaM_realloc (lua_State *L, void *block, lu_mem oldsize, lu_mem size) {
  if (size == 0) {
    l_free(block, oldsize);  /* block may be NULL; that is OK for free */
    block = NULL;
  }
  else if (size >= MAX_SIZET)
    luaD_error(L, l_s("memory allocation error: block too big"));
  else {
    block = l_realloc(block, oldsize, size);
    if (block == NULL) {
      if (L)
        luaD_breakrun(L, LUA_ERRMEM);  /* break run without error message */
      else return NULL;  /* error before creating state! */
    }
  }
  if (L && G(L)) {
    G(L)->nblocks -= oldsize;
    G(L)->nblocks += size;
  }
  return block;
}

