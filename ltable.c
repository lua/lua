/*
** $Id: ltable.c,v 1.83 2001/07/05 20:31:14 roberto Exp roberto $
** Lua tables (hash)
** See Copyright Notice in lua.h
*/


/*
** Implementation of tables (aka arrays, objects, or hash tables);
** uses a mix of chained scatter table with Brent's variation.
** A main invariant of these tables is that, if an element is not
** in its main position (i.e. the `original' position that its hash gives
** to it), then the colliding element is in its own main position.
** In other words, there are collisions only when two elements have the
** same main position (i.e. the same hash values for that table size).
** Because of that, the load factor of these tables can be 100% without
** performance penalties.
*/


#define LUA_PRIVATE
#include "lua.h"

#include "ldo.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "ltable.h"



#define TagDefault LUA_TTABLE


#define hashnum(t,n)		(node(t, lmod((lu_hash)(ls_hash)(n), t->size)))
#define hashstr(t,str)		(node(t, lmod((str)->tsv.hash, t->size)))
#define hashpointer(t,p)	(node(t, lmod(IntPoint(p), t->size)))


/*
** returns the `main' position of an element in a table (that is, the index
** of its hash value)
*/
Node *luaH_mainposition (const Hash *t, const TObject *key) {
  switch (ttype(key)) {
    case LUA_TNUMBER:
      return hashnum(t, nvalue(key));
    case LUA_TSTRING:
      return hashstr(t, tsvalue(key));
    default:  /* all other types are hashed as (void *) */
      return hashpointer(t, tsvalue(key));
  }
}


Node *luaH_next (lua_State *L, Hash *t, const TObject *key) {
  int i;
  if (ttype(key) == LUA_TNIL)
    i = 0;  /* first iteration */
  else {
    const TObject *v = luaH_get(t, key);
    if (v == &luaO_nilobject)
      luaD_error(L, l_s("invalid key for `next'"));
    i = (int)(((const lu_byte *)v -
               (const lu_byte *)(val(node(t, 0)))) / sizeof(Node)) + 1;
  }
  for (; i<t->size; i++) {
    Node *n = node(t, i);
    if (ttype(val(n)) != LUA_TNIL)
      return n;
  }
  return NULL;  /* no more elements */
}


int luaH_nexti (Hash *t, int i) {
  while ((++i)<t->size) {
    if (ttype(val(node(t, i))) != LUA_TNIL)  /* a non-nil value? */
      return i;
  }
  return -1;  /* no more elements */
}


#define check_grow(L, p, n) \
	if ((p) >= MAX_INT/(n)) luaD_error(L, l_s("table overflow"));

/*
** returns smaller power of 2 larger than `n' (minimum is MINPOWER2) 
*/
static int power2 (lua_State *L, int n) {
  int p = MINPOWER2;
  while (p <= n) {
    check_grow(L, p, 2);
    p *= 2;
  }
  return p;
}


static void setnodevector (lua_State *L, Hash *t, int size) {
  int i;
  t->node = luaM_newvector(L, size, Node);
  for (i=0; i<size; i++) {
    t->node[i].next = NULL;
    setnilvalue(key(node(t, i)));
    setnilvalue(val(node(t, i)));
  }
  t->size = size;
  t->firstfree = node(t, size-1);  /* first free position to be used */
}


Hash *luaH_new (lua_State *L, int size) {
  Hash *t = luaM_new(L, Hash);
  t->htag = TagDefault;
  t->next = G(L)->roottable;
  G(L)->roottable = t;
  t->mark = t;
  t->size = 0;
  t->weakmode = 0;
  t->node = NULL;
  setnodevector(L, t, power2(L, size));
  return t;
}


void luaH_free (lua_State *L, Hash *t) {
  luaM_freearray(L, t->node, t->size, Node);
  luaM_freelem(L, t, Hash);
}


static int numuse (const Hash *t) {
  Node *v = t->node;
  int size = t->size;
  int realuse = 0;
  int i;
  for (i=0; i<size; i++) {
    if (ttype(&v[i].val) != LUA_TNIL)
      realuse++;
  }
  return realuse;
}


static void rehash (lua_State *L, Hash *t) {
  int oldsize = t->size;
  Node *nold = t->node;
  int nelems = numuse(t);
  int i;
  lua_assert(nelems<=oldsize);
  if (nelems >= oldsize-oldsize/4) {  /* using more than 3/4? */
    check_grow(L, oldsize, 2);
    setnodevector(L, t, oldsize*2);  /* grow array */
  }
  else if (nelems <= oldsize/4 &&  /* less than 1/4? */
           oldsize > MINPOWER2)
    setnodevector(L, t, oldsize/2);  /* shrink array */
  else
    setnodevector(L, t, oldsize);  /* just rehash; keep the same size */
  for (i=0; i<oldsize; i++) {
    Node *old = nold+i;
    if (ttype(val(old)) != LUA_TNIL)
      luaH_set(L, t, key(old), val(old));
  }
  luaM_freearray(L, nold, oldsize, Node);  /* free old array */
}


/*
** inserts a new key into a hash table; first, check whether key's main 
** position is free. If not, check whether colliding node is in its main 
** position or not: if it is not, move colliding node to an empty place and 
** put new key in its main position; otherwise (colliding node is in its main 
** position), new key goes to an empty position. 
*/
static TObject *newkey (lua_State *L, Hash *t, const TObject *key) {
  Node *mp = luaH_mainposition(t, key);
  if (ttype(val(mp)) != LUA_TNIL) {  /* main position is not free? */
    Node *othern = luaH_mainposition(t, key(mp));  /* `mp' of colliding node */
    Node *n = t->firstfree;  /* get a free place */
    if (othern != mp) {  /* is colliding node out of its main position? */
      /* yes; move colliding node into free position */
      while (othern->next != mp) othern = othern->next;  /* find previous */
      othern->next = n;  /* redo the chain with `n' in place of `mp' */
      *n = *mp;  /* copy colliding node into free pos. (mp->next also goes) */
      mp->next = NULL;  /* now `mp' is free */
      setnilvalue(val(mp));
    }
    else {  /* colliding node is in its own main position */
      /* new node will go into free position */
      n->next = mp->next;  /* chain new position */
      mp->next = n;
      mp = n;
    }
  }
  setobj(key(mp), key);
  lua_assert(ttype(val(mp)) == LUA_TNIL);
  for (;;) {  /* correct `firstfree' */
    if (ttype(key(t->firstfree)) == LUA_TNIL)
      return val(mp);  /* OK; table still has a free place */
    else if (t->firstfree == t->node) break;  /* cannot decrement from here */
    else (t->firstfree)--;
  }
  rehash(L, t);  /* no more free places */
  return newkey(L, t, key);  /* `rehash' invalidates this insertion */
}


/*
** generic search function
*/
static const TObject *luaH_getany (Hash *t, const TObject *key) {
  if (ttype(key) == LUA_TNIL) return &luaO_nilobject;
  else {
    Node *n = luaH_mainposition(t, key);
    do {  /* check whether `key' is somewhere in the chain */
      if (luaO_equalObj(key(n), key)) return val(n);  /* that's it */
      else n = n->next;
    } while (n);
    return &luaO_nilobject;
  }
}


/*
** search function for integers
*/
const TObject *luaH_getnum (Hash *t, int key) {
  Node *n = hashnum(t, key);
  do {  /* check whether `key' is somewhere in the chain */
    if (ttype(key(n)) == LUA_TNUMBER && nvalue(key(n)) == (lua_Number)key)
      return val(n);  /* that's it */
    else n = n->next;
  } while (n);
  return &luaO_nilobject;
}


/*
** search function for strings
*/
const TObject *luaH_getstr (Hash *t, TString *key) {
  Node *n = hashstr(t, key);
  do {  /* check whether `key' is somewhere in the chain */
    if (ttype(key(n)) == LUA_TSTRING && tsvalue(key(n)) == key)
      return val(n);  /* that's it */
    else n = n->next;
  } while (n);
  return &luaO_nilobject;
}


/*
** main search function
*/
const TObject *luaH_get (Hash *t, const TObject *key) {
  switch (ttype(key)) {
    case LUA_TSTRING: return luaH_getstr(t, tsvalue(key));
    case LUA_TNUMBER: {
      int k = (int)nvalue(key);
      if ((lua_Number)k == nvalue(key))  /* is an integer index? */
        return luaH_getnum(t, k);  /* use specialized version */
      /* else go through */
    }
    default: return luaH_getany(t, key);
  }
}


void luaH_set (lua_State *L, Hash *t, const TObject *key, const TObject *val) {
  const TObject *p = luaH_get(t, key);
  if (p == &luaO_nilobject) {
    if (ttype(key) == LUA_TNIL) luaD_error(L, l_s("table index is nil"));
    p = newkey(L, t, key);
  }
  settableval(p, val);
}


void luaH_setstr (lua_State *L, Hash *t, TString *key, const TObject *val) {
  const TObject *p = luaH_getstr(t, key);
  if (p == &luaO_nilobject) {
    TObject k;
    setsvalue(&k, key);
    p = newkey(L, t, &k);
  }
  settableval(p, val);
}


void luaH_setnum (lua_State *L, Hash *t, int key, const TObject *val) {
  const TObject *p = luaH_getnum(t, key);
  if (p == &luaO_nilobject) {
    TObject k;
    setnvalue(&k, key);
    p = newkey(L, t, &k);
  }
  settableval(p, val);
}

