/*
** $Id: lzio.h,v 1.5 1999/08/16 20:52:00 roberto Exp roberto $
** Buffered streams
** See Copyright Notice in lua.h
*/


#ifndef lzio_h
#define lzio_h

#include <stdio.h>



/* For Lua only */
#define zFopen	luaZ_Fopen
#define zsopen	luaZ_sopen
#define zmopen	luaZ_mopen
#define zread	luaZ_read

#define EOZ	(-1)			/* end of stream */

typedef struct zio ZIO;

ZIO* zFopen (ZIO* z, FILE* f, const char *name);	/* open FILEs */
ZIO* zsopen (ZIO* z, const char* s, const char *name);	/* string */
ZIO* zmopen (ZIO* z, const char* b, size_t size, const char *name); /* memory */

size_t zread (ZIO* z, void* b, size_t n);	/* read next n bytes */

#define zgetc(z)	(((z)->n--)>0 ? ((int)*(z)->p++): (z)->filbuf(z))
#define zungetc(z)	(++(z)->n,--(z)->p)
#define zname(z)	((z)->name)


/* --------- Private Part ------------------ */

#define ZBSIZE	256			/* buffer size */

struct zio {
  size_t n;				/* bytes still unread */
  const unsigned char* p;		/* current position in buffer */
  int (*filbuf)(ZIO* z);
  void* u;				/* additional data */
  const char *name;
  unsigned char buffer[ZBSIZE];		/* buffer */
};


#endif
