/*
** $Id: lmem.h,v 1.19 2000/12/28 12:55:41 roberto Exp roberto $
** Interface to Memory Manager
** See Copyright Notice in lua.h
*/

#ifndef lmem_h
#define lmem_h


#include <stddef.h>

#include "llimits.h"
#include "lua.h"

void *luaM_realloc (lua_State *L, void *oldblock, luint32 oldsize,
                    luint32 size);

void *luaM_growaux (lua_State *L, void *block, int *size, int size_elem,
                    int limit, const char *errormsg);

#define luaM_free(L, b, s)	luaM_realloc(L, (b), (s), 0)
#define luaM_freelem(L, b, t)	luaM_realloc(L, (b), sizeof(t), 0)
#define luaM_freearray(L, b, n, t)	luaM_realloc(L, (b), \
                                          ((luint32)(n)*(luint32)sizeof(t)), 0)

#define luaM_malloc(L, t)	luaM_realloc(L, NULL, 0, (t))
#define luaM_new(L, t)          ((t *)luaM_malloc(L, sizeof(t)))
#define luaM_newvector(L, n,t)  ((t *)luaM_malloc(L, \
                                         (luint32)(n)*(luint32)sizeof(t)))

#define luaM_growvector(L,v,nelems,size,t,limit,e) \
          if (((nelems)+1) > (size)) \
            ((v)=(t *)luaM_growaux(L,v,&(size),sizeof(t),limit,e))

#define luaM_reallocvector(L, v,oldn,n,t) \
	((v)=(t *)luaM_realloc(L, v,(luint32)(oldn)*(luint32)sizeof(t), \
                                    (luint32)(n)*(luint32)sizeof(t)))


#endif

