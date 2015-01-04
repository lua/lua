/*
** $Id: lmem.h,v 1.16 2000/10/30 16:29:59 roberto Exp $
** Interface to Memory Manager
** See Copyright Notice in lua.h
*/

#ifndef lmem_h
#define lmem_h


#include <stddef.h>

#include "llimits.h"
#include "lua.h"

void *luaM_realloc (lua_State *L, void *oldblock, lint32 size);
void *luaM_growaux (lua_State *L, void *block, size_t nelems,
                    int inc, size_t size, const char *errormsg,
                    size_t limit);

#define luaM_free(L, b)		luaM_realloc(L, (b), 0)
#define luaM_malloc(L, t)	luaM_realloc(L, NULL, (t))
#define luaM_new(L, t)          ((t *)luaM_malloc(L, sizeof(t)))
#define luaM_newvector(L, n,t)  ((t *)luaM_malloc(L, (n)*(lint32)sizeof(t)))

#define luaM_growvector(L, v,nelems,inc,t,e,l) \
          ((v)=(t *)luaM_growaux(L, v,nelems,inc,sizeof(t),e,l))

#define luaM_reallocvector(L, v,n,t) \
	((v)=(t *)luaM_realloc(L, v,(n)*(lint32)sizeof(t)))


#ifdef LUA_DEBUG
extern unsigned long memdebug_numblocks;
extern unsigned long memdebug_total;
extern unsigned long memdebug_maxmem;
extern unsigned long memdebug_memlimit;
#endif


#endif

