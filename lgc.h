/*
** $Id: lgc.h,v 1.28 2003/12/09 16:56:11 roberto Exp roberto $
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
#define GCSsweepstring	3
#define GCSsweep	4
#define GCSfinalize	5


/*
** some userful bit tricks
*/
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

#define WHITE0BIT	0
#define WHITE1BIT	1
#define BLACKBIT	2
#define FINALIZEDBIT	3
#define KEYWEAKBIT	3
#define VALUEWEAKBIT	4
#define FIXEDBIT	5


#define iswhite(x)      test2bits((x)->gch.marked, WHITE0BIT, WHITE1BIT)
#define isblack(x)      testbit((x)->gch.marked, BLACKBIT)


#define otherwhite(g)	(g->currentwhite ^ bit2mask(WHITE0BIT, WHITE1BIT))
#define isdead(g,v)	((v)->gch.marked & otherwhite(g))

#define changewhite(x)	((x)->gch.marked ^= bit2mask(WHITE0BIT, WHITE1BIT))

#define valiswhite(x)	(iscollectable(x) && iswhite(gcvalue(x)))

#define luaC_white(g)	cast(lu_byte, (g)->currentwhite)


#define luaC_checkGC(L) { if (G(L)->nblocks >= G(L)->GCthreshold) \
	luaC_step(L); }


#define luaC_barrier(L,p,v) { if (valiswhite(v) && isblack(obj2gco(p)))  \
	luaC_barrierf(L,obj2gco(p),gcvalue(v)); }

#define luaC_objbarrier(L,p,o)  \
	{ if (iswhite(obj2gco(o)) && isblack(obj2gco(p))) \
		luaC_barrierf(L,obj2gco(p),obj2gco(o)); }

size_t luaC_separateudata (lua_State *L);
void luaC_callGCTM (lua_State *L);
void luaC_sweepall (lua_State *L);
void luaC_step (lua_State *L);
void luaC_link (lua_State *L, GCObject *o, lu_byte tt);
void luaC_barrierf (lua_State *L, GCObject *o, GCObject *v);


#endif
