/*
** $Id: lzio.h,v 1.4 1998/01/09 14:57:43 roberto Exp $
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

ZIO* zFopen (ZIO* z, FILE* f, char *name);		/* open FILEs */
ZIO* zsopen (ZIO* z, char* s, char *name);		/* string */
ZIO* zmopen (ZIO* z, char* b, int size, char *name);	/* memory */

int zread (ZIO* z, void* b, int n);	/* read next n bytes */

#define zgetc(z)	(--(z)->n>=0 ? ((int)*(z)->p++): (z)->filbuf(z))
#define zungetc(z)	(++(z)->n,--(z)->p)
#define zname(z)	((z)->name)


/* --------- Private Part ------------------ */

#define ZBSIZE	256			/* buffer size */

struct zio {
 int n;					/* bytes still unread */
 unsigned char* p;			/* current position in buffer */
 int (*filbuf)(ZIO* z);
 void* u;				/* additional data */
 char *name;
 unsigned char buffer[ZBSIZE];		/* buffer */
};


#endif
