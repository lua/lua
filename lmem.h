/*
** $Id: lmem.h,v 1.20 2001/02/02 15:13:05 roberto Exp roberto $
** Interface to Memory Manager
** See Copyright Notice in lua.h
*/

#ifndef lmem_h
#define lmem_h


#include <stddef.h>

#include "llimits.h"
#include "lua.h"

void *luaM_realloc (lua_State *L, void *oldblock, lu_mem oldsize, lu_mem size);

void *luaM_growaux (lua_State *L, void *block, int *size, int size_elem,
                    int limit, const char *errormsg);

#define luaM_free(L, b, s)	luaM_realloc(L, (b), (s), 0)
#define luaM_freelem(L, b, t)	luaM_realloc(L, (b), sizeof(t), 0)
#define luaM_freearray(L, b, n, t)	luaM_realloc(L, (b), \
                                          ((lu_mem)(n)*(lu_mem)sizeof(t)), 0)

#define luaM_malloc(L, t)	luaM_realloc(L, NULL, 0, (t))
#define luaM_new(L, t)          ((t *)luaM_malloc(L, sizeof(t)))
#define luaM_newvector(L, n,t)  ((t *)luaM_malloc(L, \
                                         (lu_mem)(n)*(lu_mem)sizeof(t)))

#define luaM_growvector(L,v,nelems,size,t,limit,e) \
          if (((nelems)+1) > (size)) \
            ((v)=(t *)luaM_growaux(L,v,&(size),sizeof(t),limit,e))

#define luaM_reallocvector(L, v,oldn,n,t) \
	((v)=(t *)luaM_realloc(L, v,(lu_mem)(oldn)*(lu_mem)sizeof(t), \
                                    (lu_mem)(n)*(lu_mem)sizeof(t)))


#endif

