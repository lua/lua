/*
** $Id: lmem.h,v 1.12 2000/01/13 16:30:47 roberto Exp roberto $
** Interface to Memory Manager
** See Copyright Notice in lua.h
*/

#ifndef lmem_h
#define lmem_h


#include <stdlib.h>

#include "lua.h"

/* memory error messages */
#define codeEM   "code size overflow"
#define constantEM   "constant table overflow"
#define refEM   "reference table overflow"
#define tableEM  "table overflow"
#define memEM "not enough memory"
#define arrEM	"internal array larger than `int' limit"

void *luaM_realloc (lua_State *L, void *oldblock, unsigned long size);
void *luaM_growaux (lua_State *L, void *block, unsigned long nelems, int inc, int size,
                       const char *errormsg, unsigned long limit);

#define luaM_free(L, b)	luaM_realloc(L, (b), 0)
#define luaM_malloc(L, t)	luaM_realloc(L, NULL, (t))
#define luaM_new(L, t)          ((t *)luaM_malloc(L, sizeof(t)))
#define luaM_newvector(L, n,t)  ((t *)luaM_malloc(L, (n)*sizeof(t)))
#define luaM_growvector(L, v,nelems,inc,t,e,l) \
          ((v)=(t *)luaM_growaux(L, v,nelems,inc,sizeof(t),e,l))
#define luaM_reallocvector(L, v,n,t) ((v)=(t *)luaM_realloc(L, v,(n)*sizeof(t)))


#ifdef DEBUG
extern unsigned long memdebug_numblocks;
extern unsigned long memdebug_total;
extern unsigned long memdebug_maxmem;
#endif


#endif

