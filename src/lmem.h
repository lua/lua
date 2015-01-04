/*
** $Id: lmem.h,v 1.8 1999/02/26 15:48:55 roberto Exp $
** Interface to Memory Manager
** See Copyright Notice in lua.h
*/

#ifndef lmem_h
#define lmem_h


#include <stdlib.h>

/* memory error messages */
#define codeEM   "code size overflow"
#define constantEM   "constant table overflow"
#define refEM   "reference table overflow"
#define tableEM  "table overflow"
#define memEM "not enough memory"
#define arrEM	"internal array bigger than `int' limit"

void *luaM_realloc (void *oldblock, unsigned long size);
void *luaM_growaux (void *block, unsigned long nelems, int inc, int size,
                       char *errormsg, unsigned long limit);

#define luaM_free(b)	luaM_realloc((b), 0)
#define luaM_malloc(t)	luaM_realloc(NULL, (t))
#define luaM_new(t)          ((t *)luaM_malloc(sizeof(t)))
#define luaM_newvector(n,t)  ((t *)luaM_malloc((n)*sizeof(t)))
#define luaM_growvector(v,nelems,inc,t,e,l) \
          ((v)=(t *)luaM_growaux(v,nelems,inc,sizeof(t),e,l))
#define luaM_reallocvector(v,n,t) ((v)=(t *)luaM_realloc(v,(n)*sizeof(t)))


#ifdef DEBUG
extern unsigned long numblocks;
extern unsigned long totalmem;
#endif


#endif

