/*
** $Id: lgc.h,v 1.22 2003/11/17 19:50:05 roberto Exp roberto $
** Garbage Collector
** See Copyright Notice in lua.h
*/

#ifndef lgc_h
#define lgc_h


#include "lobject.h"


/*
 * ** some userful bit tricks
 * */
#define bitmask(b)	(1<<(b))
#define setbit(x,b)	((x) |= bitmask(b))
#define resetbit(x,b)	((x) &= cast(lu_byte, ~bitmask(b)))
#define testbit(x,b)	((x) & bitmask(b))



/*
** Layout for bit use in `marked' field:
** bit 0 - object is gray
** bit 1 - object is black
** bit 2 - For userdata: is finalized;
           for tables: has weak keys
** bit 3 - for tables: has weak values
** bit 4 - object is fixed (should not be collected)
*/

#define GRAYBIT		0
#define BLACKBIT	1
#define FINALIZEDBIT	2
#define KEYWEAKBIT	2
#define VALUEWEAKBIT	3
#define FIXEDBIT	4



#define luaC_checkGC(L) { if (G(L)->nblocks >= G(L)->GCthreshold) \
	luaC_collectgarbage(L); }


size_t luaC_separateudata (lua_State *L);
void luaC_callGCTM (lua_State *L);
void luaC_sweep (lua_State *L, int all);
void luaC_collectgarbage (lua_State *L);
void luaC_link (lua_State *L, GCObject *o, lu_byte tt);


#endif
