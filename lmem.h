/*
** $Id: lmem.h,v 1.27 2004/11/19 15:52:40 roberto Exp roberto $
** Interface to Memory Manager
** See Copyright Notice in lua.h
*/

#ifndef lmem_h
#define lmem_h


#include <stddef.h>

#include "llimits.h"
#include "lua.h"

#define MEMERRMSG	"not enough memory"


void *luaM_realloc (lua_State *L, void *block, size_t oldsize, size_t size);

void *luaM_toobig (lua_State *L);

#define luaM_reallocv(L,b,on,n,e) \
  ((cast(size_t, (n)+1) <= MAX_SIZET/(e)) ?  /* +1 only to avoid warnings */ \
    luaM_realloc(L, (b), (on)*(e), (n)*(e)) : \
    luaM_toobig(L))


void *luaM_growaux (lua_State *L, void *block, int *size, size_t size_elem,
                    int limit, const char *errormsg);

#define luaM_freemem(L, b, s)	luaM_realloc(L, (b), (s), 0)
#define luaM_free(L, b)		luaM_realloc(L, (b), sizeof(*(b)), 0)
#define luaM_freearray(L, b, n, t)   luaM_reallocv(L, (b), n, 0, sizeof(t))

#define luaM_malloc(L,t)	luaM_realloc(L, NULL, 0, (t))
#define luaM_new(L,t)		cast(t *, luaM_malloc(L, sizeof(t)))
#define luaM_newvector(L,n,t) \
		cast(t *, luaM_reallocv(L, NULL, 0, n, sizeof(t)))

#define luaM_growvector(L,v,nelems,size,t,limit,e) \
          if (((nelems)+1) > (size)) \
            ((v)=cast(t *, luaM_growaux(L,v,&(size),sizeof(t),limit,e)))

#define luaM_reallocvector(L, v,oldn,n,t) \
   ((v)=cast(t *, luaM_reallocv(L, v, oldn, n, sizeof(t))))


#endif

