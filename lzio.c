/*
** $Id: lzio.c,v 1.8 1999/08/16 20:52:00 roberto Exp roberto $
** a generic input stream interface
** See Copyright Notice in lua.h
*/



#include <stdio.h>
#include <string.h>

#include "lzio.h"



/* ----------------------------------------------------- memory buffers --- */

static int zmfilbuf (ZIO* z) {
  (void)z;  /* to avoid warnings */
  return EOZ;
}


ZIO* zmopen (ZIO* z, const char* b, int size, const char *name) {
  if (b==NULL) return NULL;
  z->n = size;
  z->p = (unsigned const char *)b;
  z->filbuf = zmfilbuf;
  z->u = NULL;
  z->name = name;
  return z;
}

/* ------------------------------------------------------------ strings --- */

ZIO* zsopen (ZIO* z, const char* s, const char *name) {
  if (s==NULL) return NULL;
  return zmopen(z,s,strlen(s),name);
}

/* -------------------------------------------------------------- FILEs --- */

static int zffilbuf (ZIO* z) {
  int n;
  if (feof((FILE *)z->u)) return EOZ;
  n = fread(z->buffer,1,ZBSIZE,z->u);
  if (n==0) return EOZ;
  z->n = n-1;
  z->p = z->buffer;
  return *(z->p++);
}


ZIO* zFopen (ZIO* z, FILE* f, const char *name) {
  if (f==NULL) return NULL;
  z->n = 0;
  z->p = z->buffer;
  z->filbuf = zffilbuf;
  z->u = f;
  z->name = name;
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
