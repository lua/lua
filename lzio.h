/*
** $Id: lzio.h,v 1.10 2002/06/03 17:46:34 roberto Exp roberto $
** Buffered streams
** See Copyright Notice in lua.h
*/


#ifndef lzio_h
#define lzio_h

#include "lua.h"


/* For Lua only */
#define zread	luaZ_zread

#define EOZ	(-1)			/* end of stream */

typedef struct zio ZIO;

#define zgetc(z)  (((z)->n--)>0 ? \
			cast(int, cast(unsigned char, *(z)->p++)) : \
			luaZ_fill(z))

#define zname(z)	((z)->name)

void luaZ_init (ZIO *z, lua_Getblock getblock, void *ud, const char *name);
size_t luaZ_zread (ZIO* z, void* b, size_t n);	/* read next n bytes */
int luaZ_lookahead (ZIO *z);


/* --------- Private Part ------------------ */

struct zio {
  size_t n;			/* bytes still unread */
  const char *p;		/* current position in buffer */
  lua_Getblock getblock;
  void* ud;			/* additional data */
  const char *name;
};


int luaZ_fill (ZIO *z);

#endif
