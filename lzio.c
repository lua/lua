/*
** $Id: lzio.c,v 1.17 2002/06/03 17:46:34 roberto Exp roberto $
** a generic input stream interface
** See Copyright Notice in lua.h
*/


#include <string.h>

#include "lua.h"

#include "llimits.h"
#include "lzio.h"



int luaZ_fill (ZIO *z) {
  size_t size;
  const char *buff = z->getblock(z->ud, &size);
  if (buff == NULL || size == 0) return EOZ;
  z->n = size - 1;
  z->p = buff;
  return *(z->p++);
}


int luaZ_lookahead (ZIO *z) {
  if (z->n == 0) {
    int c = luaZ_fill(z);
    if (c == EOZ) return c;
    z->n++;
    z->p--;
  }
  return *z->p;
}


void luaZ_init (ZIO *z, lua_Getblock getblock, void *ud, const char *name) {
  z->getblock = getblock;
  z->ud = ud;
  z->name = name;
  z->n = 0;
  z->p = NULL;
}


/* --------------------------------------------------------------- read --- */
size_t luaZ_zread (ZIO *z, void *b, size_t n) {
  while (n) {
    size_t m;
    if (z->n == 0) {
      if (luaZ_fill(z) == EOZ)
        return n;  /* return number of missing bytes */
      else {
        ++z->n;  /* filbuf removed first byte; put back it */
        --z->p;
      }
    }
    m = (n <= z->n) ? n : z->n;  /* min. between n and z->n */
    memcpy(b, z->p, m);
    z->n -= m;
    z->p += m;
    b = (char *)b + m;
    n -= m;
  }
  return 0;
}

