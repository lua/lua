/*
** $Id: lzio.h,v 1.12 2002/06/06 12:40:22 roberto Exp roberto $
** Buffered streams
** See Copyright Notice in lua.h
*/


#ifndef lzio_h
#define lzio_h

#include "lua.h"


#define EOZ	(-1)			/* end of stream */

typedef struct zio ZIO;

#define zgetc(z)  (((z)->n--)>0 ? \
			cast(int, cast(unsigned char, *(z)->p++)) : \
			luaZ_fill(z))

#define zname(z)	((z)->name)

void luaZ_init (ZIO *z, lua_Chunkreader reader, void *data, const char *name);
size_t luaZ_read (ZIO* z, void* b, size_t n);	/* read next n bytes */
int luaZ_lookahead (ZIO *z);


/* --------- Private Part ------------------ */

struct zio {
  size_t n;			/* bytes still unread */
  const char *p;		/* current position in buffer */
  lua_Chunkreader reader;
  void* data;			/* additional data */
  const char *name;
};


int luaZ_fill (ZIO *z);

#endif
