/*
** $Id: lmem.h,v 1.17 2000/11/24 17:39:56 roberto Exp roberto $
** Interface to Memory Manager
** See Copyright Notice in lua.h
*/

#ifndef lmem_h
#define lmem_h


#include <stddef.h>

#include "llimits.h"
#include "lua.h"

void *luaM_realloc (lua_State *L, void *oldblock, luint32 size);
void *luaM_growaux (lua_State *L, void *block, int *size, int size_elem,
                    int limit, const char *errormsg);

#define luaM_free(L, b)		luaM_realloc(L, (b), 0)
#define luaM_malloc(L, t)	luaM_realloc(L, NULL, (t))
#define luaM_new(L, t)          ((t *)luaM_malloc(L, sizeof(t)))
#define luaM_newvector(L, n,t)  ((t *)luaM_malloc(L, (n)*(luint32)sizeof(t)))

#define luaM_growvector(L,v,nelems,size,t,limit,e) \
          if (((nelems)+1) > (size)) \
            ((v)=(t *)luaM_growaux(L,v,&(size),sizeof(t),limit,e))

#define luaM_reallocvector(L, v,n,t) \
	((v)=(t *)luaM_realloc(L, v,(n)*(luint32)sizeof(t)))


#ifdef LUA_DEBUG
extern mem_int memdebug_numblocks;
extern mem_int memdebug_total;
extern mem_int memdebug_maxmem;
extern mem_int memdebug_memlimit;
#endif


#endif

