/*
* zio.h
* a generic input stream interface
* $Id: zio.h,v 1.1 1997/06/16 16:50:22 roberto Exp roberto $
*/

#ifndef zio_h
#define zio_h

#include <stdio.h>



/* For Lua only */
#define zFopen	luaz_Fopen
#define zfopen	luaz_fopen
#define zpopen	luaz_popen
#define zsopen	luaz_sopen
#define zmopen	luaz_mopen

#define EOZ	(-1)			/* end of stream */

typedef struct zio ZIO;

int zgetc(ZIO* z);			/* get next byte */
int zungetc(ZIO* z);			/* put back last byte read */
int zread(ZIO* z, void* b, int n);	/* read next n bytes */
int zclose(ZIO* z);			/* close stream */

ZIO* zFopen(ZIO* z, FILE* f);		/* open FILEs */
ZIO* zfopen(ZIO* z, char* s, char* m);	/* file by name */
ZIO* zpopen(ZIO* z, char* s, char* m);	/* pipe */
ZIO* zsopen(ZIO* z, char* s);		/* string */
ZIO* zmopen(ZIO* z, char* b, int size);	/* memory */

#define zgetc(z)	(--(z)->n>=0 ? ((int)*(z)->p++): (z)->filbuf(z))
#define zungetc(z)	(++(z)->n,--(z)->p)
#define zclose(z)	(*(z)->close)(z)



/* --------- Private Part ------------------ */

#define ZBSIZE	256			/* buffer size */

struct zio {
 int n;					/* bytes still unread */
 unsigned char* p;			/* current position in buffer */
 int (*filbuf)(ZIO* z);
 int (*close)(ZIO* z);
 void* u;				/* additional data */
 unsigned char buffer[ZBSIZE];		/* buffer */
};


#endif
