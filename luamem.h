/*
** mem.c
** memory manager for lua
** $Id: mem.h,v 1.4 1996/03/14 15:55:49 roberto Exp roberto $
*/
 
#ifndef mem_h
#define mem_h

#ifndef NULL
#define NULL 0
#endif


/* memory error messages */
#define NUMERRMSG 6
enum memerrormsg {codeEM, symbolEM, constantEM, stackEM, lexEM, lockEM};
extern char *luaI_memerrormsg[];


void luaI_free (void *block);
void *luaI_malloc (unsigned long size);
void *luaI_realloc (void *oldblock, unsigned long size);
void *luaI_buffer (unsigned long size);
int luaI_growvector (void **block, unsigned long nelems, int size,
                       enum memerrormsg errormsg, unsigned long limit);

#define new(s)          ((s *)luaI_malloc(sizeof(s)))
#define newvector(n,s)  ((s *)luaI_malloc((n)*sizeof(s)))
#define growvector(old,n,s,e,l) \
          (luaI_growvector((void**)old,n,sizeof(s),e,l))

#endif 

