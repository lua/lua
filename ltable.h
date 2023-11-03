/*
** $Id: ltable.h $
** Lua tables (hash)
** See Copyright Notice in lua.h
*/

#ifndef ltable_h
#define ltable_h

#include "lobject.h"


#define gnode(t,i)	(&(t)->node[i])
#define gval(n)		(&(n)->i_val)
#define gnext(n)	((n)->u.next)


/*
** Clear all bits of fast-access metamethods, which means that the table
** may have any of these metamethods. (First access that fails after the
** clearing will set the bit again.)
*/
#define invalidateTMcache(t)	((t)->flags &= ~maskflags)


/* true when 't' is using 'dummynode' as its hash part */
#define isdummy(t)		((t)->lastfree == NULL)


/* allocated size for hash nodes */
#define allocsizenode(t)	(isdummy(t) ? 0 : sizenode(t))


/* returns the Node, given the value of a table entry */
#define nodefromval(v)	cast(Node *, (v))


/* results from get/pset */
#define HOK		0
#define HNOTFOUND	1
#define HNOTATABLE	2
#define HFIRSTNODE	3

/*
** Besides these values, pset (pre-set) operations may also return an
** encoding of where the value should go (usually called 'hres'). That
** means that there is a slot with that key but with no value. (pset
** cannot set that value because there might be a metamethod.) If the
** slot is in the hash part, the encoding is (HFIRSTNODE + hash index);
** if the slot is in the array part, the encoding is (~array index).
*/


/*
** The array part of a table is represented by an array of cells.
** Each cell is composed of (NM + 1) elements, and each element has the
** type 'ArrayCell'.  In each cell, only one element has the variant
** 'tag', while the other NM elements have the variant 'value'. The
** array in the 'tag' element holds the tags of the other elements in
** that cell.
*/
#define NM      ((unsigned int)sizeof(Value))

union ArrayCell {
  unsigned char tag[NM];
  Value value;
};


/*
** 'NMTag' defines which cell element has the tags; that could be any
** value between 0 (tags come before all values) and NM (tags come after
** all values).
*/
#define NMTag     0


/*
** Computes the concrete index that holds the tag of abstract index 'i'
*/
#define TagIndex(i)     (((i)/NM * (NM + 1u)) + NMTag)

/*
** Computes the concrete index that holds the value of abstract index 'i'
*/
#define ValueIndex(i)   ((i) + (((i) + (NM - NMTag))/NM))


/* Computes the address of the tag for the abstract index 'k' */
#define getArrTag(t,k)	(&(t)->array[TagIndex(k)].tag[(k)%NM])

/* Computes the address of the value for the abstract index 'k' */
#define getArrVal(t,k)	(&(t)->array[ValueIndex(k)].value)


/*
** Move TValues to/from arrays, using Lua indices
*/
#define arr2obj(h,k,val)  \
  ((val)->tt_ = *getArrTag(h,(k)-1u), (val)->value_ = *getArrVal(h,(k)-1u))

#define obj2arr(h,k,val)  \
  (*getArrTag(h,(k)-1u) = (val)->tt_, *getArrVal(h,(k)-1u) = (val)->value_)


/*
** Often, we need to check the tag of a value before moving it. These
** macros also move TValues to/from arrays, but receive the precomputed
** tag value or address as an extra argument.
*/
#define farr2val(h,k,tag,res)  \
  ((res)->tt_ = tag, (res)->value_ = *getArrVal(h,(k)-1u))

#define fval2arr(h,k,tag,val)  \
  (*tag = (val)->tt_, *getArrVal(h,(k)-1u) = (val)->value_)


LUAI_FUNC int luaH_getshortstr (Table *t, TString *key, TValue *res);
LUAI_FUNC int luaH_getstr (Table *t, TString *key, TValue *res);
LUAI_FUNC int luaH_get (Table *t, const TValue *key, TValue *res);
LUAI_FUNC int luaH_getint (Table *t, lua_Integer key, TValue *res);

LUAI_FUNC TString *luaH_getstrkey (Table *t, TString *key);

LUAI_FUNC int luaH_psetint (Table *t, lua_Integer key, TValue *val);
LUAI_FUNC int luaH_psetshortstr (Table *t, TString *key, TValue *val);
LUAI_FUNC int luaH_psetstr (Table *t, TString *key, TValue *val);
LUAI_FUNC int luaH_pset (Table *t, const TValue *key, TValue *val);

LUAI_FUNC void luaH_setint (lua_State *L, Table *t, lua_Integer key,
                                                    TValue *value);
LUAI_FUNC const TValue *luaH_Hgetshortstr (Table *t, TString *key);
LUAI_FUNC void luaH_newkey (lua_State *L, Table *t, const TValue *key,
                                                    TValue *value);
LUAI_FUNC void luaH_set (lua_State *L, Table *t, const TValue *key,
                                                 TValue *value);
LUAI_FUNC void luaH_finishset (lua_State *L, Table *t, const TValue *key,
                                              TValue *value, int aux);
LUAI_FUNC Table *luaH_new (lua_State *L);
LUAI_FUNC void luaH_resize (lua_State *L, Table *t, unsigned int nasize,
                                                    unsigned int nhsize);
LUAI_FUNC void luaH_resizearray (lua_State *L, Table *t, unsigned int nasize);
LUAI_FUNC void luaH_free (lua_State *L, Table *t);
LUAI_FUNC int luaH_next (lua_State *L, Table *t, StkId key);
LUAI_FUNC lua_Unsigned luaH_getn (Table *t);
LUAI_FUNC unsigned int luaH_realasize (const Table *t);


#if defined(LUA_DEBUG)
LUAI_FUNC Node *luaH_mainposition (const Table *t, const TValue *key);
#endif


#endif
