/*
** $Id: lzio.c,v 1.4 1998/12/28 13:44:54 roberto Exp roberto $
** a generic input stream interface
** See Copyright Notice in lua.h
*/



#include <stdio.h>
#include <string.h>

#include "lzio.h"



/* ----------------------------------------------------- memory buffers --- */

static int zmfilbuf (ZIO* z) {
 return EOZ;
}


ZIO* zmopen (ZIO* z, char* b, int size, char *name)
{
 if (b==NULL) return NULL;
 z->n=size;
 z->p= (unsigned char *)b;
 z->filbuf=zmfilbuf;
 z->u=NULL;
 z->name=name;
 return z;
}

/* ------------------------------------------------------------ strings --- */

ZIO* zsopen (ZIO* z, char* s, char *name)
{
 if (s==NULL) return NULL;
 return zmopen(z,s,strlen(s),name);
}

/* -------------------------------------------------------------- FILEs --- */

static int zffilbuf (ZIO* z)
{
 int n=fread(z->buffer,1,ZBSIZE,z->u);
 if (n==0) return EOZ;
 z->n=n-1;
 z->p=z->buffer;
 return *(z->p++);
}


ZIO* zFopen (ZIO* z, FILE* f, char *name)
{
 if (f==NULL) return NULL;
 z->n=0;
 z->p=z->buffer;
 z->filbuf=zffilbuf;
 z->u=f;
 z->name=name;
 return z;
}


/* --------------------------------------------------------------- read --- */
int zread (ZIO *z, void *b, int n) {
  while (n) {
    int m;
    if (z->n == 0) {
      if (z->filbuf(z) == EOZ)
        return n;  /* return number of missing bytes */
      zungetc(z);  /* put result from 'filbuf' in the buffer */
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
