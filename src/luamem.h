/*
** mem.c
** memory manager for lua
** $Id: luamem.h,v 1.9 1997/03/31 14:10:11 roberto Exp $
*/
 
#ifndef luamem_h
#define luamem_h

#ifndef NULL
#define NULL 0
#endif


/* memory error messages */
#define codeEM   "code size overflow"
#define symbolEM   "symbol table overflow"
#define constantEM   "constant table overflow"
#define stackEM   "stack size overflow"
#define lexEM   "lex buffer overflow"
#define refEM   "reference table overflow"
#define tableEM  "table overflow"
#define memEM "not enough memory"


void luaI_free (void *block);
void *luaI_realloc (void *oldblock, unsigned long size);
void *luaI_buffer (unsigned long size);
int luaI_growvector (void **block, unsigned long nelems, int size,
                       char *errormsg, unsigned long limit);

#define luaI_malloc(s)	luaI_realloc(NULL, (s))
#define new(s)          ((s *)luaI_malloc(sizeof(s)))
#define newvector(n,s)  ((s *)luaI_malloc((n)*sizeof(s)))
#define growvector(old,n,s,e,l) \
          (luaI_growvector((void**)old,n,sizeof(s),e,l))

#endif 

