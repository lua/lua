/*
** mem.c
** memory manager for lua
** $Id: $
*/
 
#ifndef mem_h
#define mem_h

#ifndef NULL
#define NULL 0
#endif

void luaI_free (void *block);
void *luaI_malloc (unsigned long size);
void *luaI_realloc (void *oldblock, unsigned long size);

#define new(s)          ((s *)luaI_malloc(sizeof(s)))
#define newvector(n,s)  ((s *)luaI_malloc((n)*sizeof(s)))
#define growvector(old,n,s) ((s *)luaI_realloc(old,(n)*sizeof(s)))

#endif 

