/*
** $Id: lgc.h,v 1.24 2003/11/19 19:41:57 roberto Exp roberto $
** Garbage Collector
** See Copyright Notice in lua.h
*/

#ifndef lgc_h
#define lgc_h


#include "lobject.h"


/*
** Possible states of the Garbage Collector
*/
#define GCSroot		0
#define GCSpropagate	1
#define GCSatomic	2
#define GCSsweep	3
#define GCSfinalize	4


/*
 * ** some userful bit tricks
 * */
#define resetbits(x,m)	((x) &= cast(lu_byte, ~(m)))
#define setbits(x,m)	((x) |= (m))
#define testbits(x,m)	((x) & (m))
#define bitmask(b)	(1<<(b))
#define bit2mask(b1,b2)	(bitmask(b1) | bitmask(b2))
#define setbit(x,b)	setbits(x, bitmask(b))
#define resetbit(x,b)	resetbits(x, bitmask(b))
#define testbit(x,b)	testbits(x, bitmask(b))
#define set2bits(x,b1,b2)	setbits(x, (bit2mask(b1, b2)))
#define reset2bits(x,b1,b2)	resetbits(x, (bit2mask(b1, b2)))
#define test2bits(x,b1,b2)	testbits(x, (bit2mask(b1, b2)))



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
void luaC_sweepall (lua_State *L);
void luaC_collectgarbage (lua_State *L);
void luaC_link (lua_State *L, GCObject *o, lu_byte tt);


#endif
