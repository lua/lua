/*
** $Id: lmem.h,v 1.5 1997/12/17 20:48:58 roberto Exp $
** Interface to Memory Manager
** See Copyright Notice in lua.h
*/

#ifndef lmem_h
#define lmem_h


#ifndef NULL
#define NULL 0
#endif


/* memory error messages */
#define codeEM   "code size overflow"
#define constantEM   "constant table overflow"
#define refEM   "reference table overflow"
#define tableEM  "table overflow"
#define memEM "not enough memory"

void *luaM_realloc (void *oldblock, unsigned long size);
int luaM_growaux (void **block, unsigned long nelems, int size,
                       char *errormsg, unsigned long limit);

#define luaM_free(b)	luaM_realloc((b), 0)
#define luaM_malloc(t)	luaM_realloc(NULL, (t))
#define luaM_new(t)          ((t *)luaM_malloc(sizeof(t)))
#define luaM_newvector(n,t)  ((t *)luaM_malloc((n)*sizeof(t)))
#define luaM_growvector(old,n,t,e,l) \
          (luaM_growaux((void**)old,n,sizeof(t),e,l))
#define luaM_reallocvector(v,n,t) ((t *)luaM_realloc(v,(n)*sizeof(t)))


#ifdef DEBUG
extern unsigned long numblocks;
extern unsigned long totalmem;
#endif


#endif

