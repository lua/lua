/*
* zio.c
* a generic input stream interface
* $Id: zio.c,v 1.2 1997/06/20 19:25:54 roberto Exp $
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zio.h"


/* ----------------------------------------------------- memory buffers --- */

static int zmfilbuf(ZIO* z)
{
 return EOZ;
}

ZIO* zmopen(ZIO* z, char* b, int size)
{
 if (b==NULL) return NULL;
 z->n=size;
 z->p= (unsigned char *)b;
 z->filbuf=zmfilbuf;
 z->u=NULL;
 return z;
}

/* ------------------------------------------------------------ strings --- */

ZIO* zsopen(ZIO* z, char* s)
{
 if (s==NULL) return NULL;
 return zmopen(z,s,strlen(s));
}

/* -------------------------------------------------------------- FILEs --- */

static int zffilbuf(ZIO* z)
{
 int n=fread(z->buffer,1,ZBSIZE,z->u);
 if (n==0) return EOZ;
 z->n=n-1;
 z->p=z->buffer;
 return *(z->p++);
}


ZIO* zFopen(ZIO* z, FILE* f)
{
 if (f==NULL) return NULL;
 z->n=0;
 z->p=z->buffer;
 z->filbuf=zffilbuf;
 z->u=f;
 return z;
}


/* --------------------------------------------------------------- read --- */
int zread(ZIO *z, void *b, int n)
{
  while (n) {
    int m;
    if (z->n == 0) {
      if (z->filbuf(z) == EOZ)
        return n;  /* retorna quantos faltaram ler */
      zungetc(z);  /* poe o resultado de filbuf no buffer */
    }
    m = (n <= z->n) ? n : z->n;  /* minimo de n e z->n */
    memcpy(b, z->p, m);
    z->n -= m;
    z->p += m;
    b = (char *)b + m;
    n -= m;
  }
  return 0;
}
