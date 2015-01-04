/*
** mem.c
** memory manager for lua
** $Id: mem.h,v 1.7 1996/04/22 18:00:37 roberto Exp $
*/
 
#ifndef mem_h
#define mem_h

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
void *luaI_malloc (unsigned long size);
void *luaI_realloc (void *oldblock, unsigned long size);
void *luaI_buffer (unsigned long size);
int luaI_growvector (void **block, unsigned long nelems, int size,
                       char *errormsg, unsigned long limit);

#define new(s)          ((s *)luaI_malloc(sizeof(s)))
#define newvector(n,s)  ((s *)luaI_malloc((n)*sizeof(s)))
#define growvector(old,n,s,e,l) \
          (luaI_growvector((void**)old,n,sizeof(s),e,l))

#endif 

