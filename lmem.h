/*
** $Id: lmem.h,v 1.6 1998/12/15 14:59:43 roberto Exp roberto $
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

void *luaM_realloc (void *oldblock, unsigned long size);
void *luaM_growaux (void *block, unsigned long nelems, int inc, int size,
                       char *errormsg, unsigned long limit);

#define luaM_free(b)	luaM_realloc((b), 0)
#define luaM_malloc(t)	luaM_realloc(NULL, (t))
#define luaM_new(t)          ((t *)luaM_malloc(sizeof(t)))
#define luaM_newvector(n,t)  ((t *)luaM_malloc((n)*sizeof(t)))
#define luaM_growvector(old,nelems,inc,t,e,l) \
          ((t *)luaM_growaux(old,nelems,inc,sizeof(t),e,l))
#define luaM_reallocvector(v,n,t) ((t *)luaM_realloc(v,(n)*sizeof(t)))


#ifdef DEBUG
extern unsigned long numblocks;
extern unsigned long totalmem;
#endif


#endif

