/*
** $Id: ltable.c,v 1.1 2001/11/29 22:14:34 rieru Exp rieru $
** Lua tables (hash)
** See Copyright Notice in lua.h
*/


/*
** Implementation of tables (aka arrays, objects, or hash tables).
** Tables keep its elements in two parts: an array part and a hash part.
** Non-negative integer keys are all candidates to be kept in the array
** part. The actual size of the array is the largest `n' such that at
** least half the slots between 0 and n are in use.
** Hash uses a mix of chained scatter table with Brent's variation.
** A main invariant of these tables is that, if an element is not
** in its main position (i.e. the `original' position that its hash gives
** to it), then the colliding element is in its own main position.
** In other words, there are collisions only when two elements have the
** same main position (i.e. the same hash values for that table size).
** Because of that, the load factor of these tables can be 100% without
** performance penalties.
*/



#include "lua.h"

#include "ldo.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "ltable.h"


/*
** max size of array part is 2^MAXBITS
*/
#if BITS_INT > 26
#define MAXBITS		24
#else
#define MAXBITS		(BITS_INT-2)
#endif

/* check whether `x' < 2^MAXBITS */
#define toobig(x)	((((x)-1) >> MAXBITS) != 0)



#define TagDefault LUA_TTABLE


#define hashnum(t,n)	\
           (node(t, lmod(cast(lu_hash, cast(ls_hash, n)), sizenode(t))))
#define hashstr(t,str)	 (node(t, lmod((str)->tsv.hash, sizenode(t))))
#define hashboolean(t,p) (node(t, lmod(p, sizenode(t))))
#define hashpointer(t,p) (node(t, lmod(IntPoint(p), sizenode(t))))


/*
** returns the `main' position of an element in a table (that is, the index
** of its hash value)
*/
Node *luaH_mainposition (const Table *t, const TObject *key) {
  switch (ttype(key)) {
    case LUA_TNUMBER:
      return hashnum(t, nvalue(key));
    case LUA_TSTRING:
      return hashstr(t, tsvalue(key));
    case LUA_TBOOLEAN:
      return hashboolean(t, bvalue(key));
    default:  /* all other types are hashed as (void *) */
      return hashpointer(t, tsvalue(key));
  }
}


/*
** returns the index for `key' if `key' is an appropriate key to live in
** the array part of the table, -1 otherwise.
*/
static int arrayindex (const TObject *key) {
  if (ttype(key) == LUA_TNUMBER) {
    int k = cast(int, nvalue(key));
    if (cast(lua_Number, k) == nvalue(key) && k >= 1 && !toobig(k))
      return k;
  }
  return -1;  /* `key' did not match some condition */
}


/*
** returns the index of a `key' for table traversals. First goes all
** elements in the array part, then elements in the hash part. The
** beginning and end of a traversal are signalled by -1.
*/
int luaH_index (lua_State *L, Table *t, const TObject *key) {
  int i;
  if (ttype(key) == LUA_TNIL) return -1;  /* first iteration */
  i = arrayindex(key);
  if (0 <= i && i <= t->sizearray) {  /* is `key' inside array part? */
    return i-1;  /* yes; that's the index (corrected to C) */
  }
  else {
    const TObject *v = luaH_get(t, key);
    if (v == &luaO_nilobject)
      luaD_error(L, "invalid key for `next'");
    i = cast(int, (cast(const lu_byte *, v) -
                   cast(const lu_byte *, val(node(t, 0)))) / sizeof(Node));
    return i + t->sizearray;  /* hash elements are numbered after array ones */
  }
}


int luaH_nexti (Table *t, int i, TObject *where) {
  for (i++; i < t->sizearray; i++) {  /* try first array part */
    if (ttype(&t->array[i]) != LUA_TNIL) {  /* a non-nil value? */
      setnvalue(where, i+1);
      setobj(where+1, &t->array[i]);
      return i;
    }
  }
  for (i -= t->sizearray; i < sizenode(t); i++) {  /* then hash part */
    if (ttype(val(node(t, i))) != LUA_TNIL) {  /* a non-nil value? */
      setobj(where, key(node(t, i)));
      setobj(where+1, val(node(t, i)));
      return i + t->sizearray;
    }
  }
  return -1;  /* no more elements */
}


/*
** {=============================================================
** Rehash
** ==============================================================
*/


static void computesizes  (int nums[], int ntotal, int *narray, int *nhash) {
  int n = 0;  /* (log of) optimal size for array part */
  int na = 0;  /* number of elements to go to array part */
  int i;
  int a = nums[0];  /* number of elements smaller than 2^i */
  for (i = 1; i <= MAXBITS && *narray >= twoto(i-1); i++) {
    if (nums[i] == 0) continue;
    a += nums[i];
    if (a >= twoto(i-1)) {  /* more than half elements in use? */
      n = i;
      na = a;
    }
  }
  lua_assert(na <= *narray && *narray <= ntotal);
  *nhash = ntotal - na;
  *narray = (n == 0) ? 0 : twoto(n);
  lua_assert(na <= *narray && na >= *narray/2);
}


static void numuse (const Table *t, int *narray, int *nhash) {
  int nums[MAXBITS+1];
  int i;
  int totaluse = 0;
  for (i=0; i<=MAXBITS; i++) nums[i] = 0;  /* init `nums' */
  /* count elements in array part */
  i = luaO_log2(t->sizearray) + 1;  /* number of `slices' */
  while (i--) {  /* for each slice [2^(i-1) to 2^i) */
    int to = twoto(i);
    int from = to/2;
    if (to > t->sizearray) to = t->sizearray;
    for (; from < to; from++)
      if (ttype(&t->array[from]) != LUA_TNIL) {
        nums[i]++;
        totaluse++;
      }
  }
  *narray = totaluse;  /* all previous uses were in array part */
  /* count elements in hash part */
  i = sizenode(t);
  while (i--) {
    if (ttype(val(&t->node[i])) != LUA_TNIL) {
      int k = arrayindex(key(&t->node[i]));
      if (k >= 0) {  /* is `key' an appropriate array index? */
        nums[luaO_log2(k-1)+1]++;  /* count as such */
        (*narray)++;
      }
      totaluse++;
    }
  }
  computesizes(nums, totaluse, narray, nhash);
}


/*
** (log of) minimum size for hash part of a table
*/
#define MINHASHSIZE	1


static void setarrayvector (lua_State *L, Table *t, int size) {
  int i;
  luaM_reallocvector(L, t->array, t->sizearray, size, TObject);
  for (i=t->sizearray; i<size; i++)
     setnilvalue(&t->array[i]);
  t->sizearray = size;
}


static void setnodevector (lua_State *L, Table *t, int lsize) {
  int i;
  int size;
  if (lsize < MINHASHSIZE) lsize = MINHASHSIZE;
  else if (lsize > MAXBITS)
    luaD_error(L, "table overflow");
  size = twoto(lsize);
  t->node = luaM_newvector(L, size, Node);
  for (i=0; i<size; i++) {
    t->node[i].next = NULL;
    setnilvalue(key(node(t, i)));
    setnilvalue(val(node(t, i)));
  }
  t->lsizenode = cast(lu_byte, lsize);
  t->firstfree = node(t, size-1);  /* first free position to be used */
}


static void resize (lua_State *L, Table *t, int nasize, int nhsize) {
  int i;
  int oldasize, oldhsize;
  Node *nold;
  oldasize = t->sizearray;
  if (nasize > oldasize)  /* should grow array part? */
    setarrayvector(L, t, nasize);
  /* create new hash part with appropriate size */
  nold = t->node;  /* save old hash ... */
  oldhsize = t->lsizenode;  /* ... and (log of) old size */
  setnodevector(L, t, nhsize);  
  /* re-insert elements */
  if (nasize < oldasize) {  /* array part must shrink? */
    t->sizearray = nasize;
    /* re-insert elements from vanishing slice */
    for (i=nasize; i<oldasize; i++) {
      if (ttype(&t->array[i]) != LUA_TNIL)
        luaH_setnum(L, t, i+1, &t->array[i]);
    }
    /* shink array */
    luaM_reallocvector(L, t->array, oldasize, nasize, TObject);
  }
  /* re-insert elements in hash part */
  i = twoto(oldhsize);
  while (i--) {
    Node *old = nold+i;
    if (ttype(val(old)) != LUA_TNIL)
      luaH_set(L, t, key(old), val(old));
  }
  luaM_freearray(L, nold, twoto(oldhsize), Node);  /* free old array */
}


static void rehash (lua_State *L, Table *t) {
  int nasize, nhsize;
  numuse(t, &nasize, &nhsize);  /* compute new sizes for array and hash parts */
  nhsize += nhsize/4;  /* allow some extra for growing nhsize */
  resize(L, t, nasize, luaO_log2(nhsize)+1);
}



/*
** }=============================================================
*/


Table *luaH_new (lua_State *L, int narray, int lnhash) {
  Table *t = luaM_new(L, Table);
  t->eventtable = hvalue(defaultet(L));
  t->next = G(L)->roottable;
  G(L)->roottable = t;
  t->mark = t;
  t->flags = cast(unsigned short, ~0);
  /* temporary values (kept only if some malloc fails) */
  t->array = NULL;
  t->sizearray = 0;
  t->lsizenode = 0;
  t->node = NULL;
  setarrayvector(L, t, narray);
  setnodevector(L, t, lnhash);
  return t;
}


void luaH_free (lua_State *L, Table *t) {
  lua_assert(t->lsizenode > 0 || t->node == NULL);
  if (t->lsizenode > 0)
    luaM_freearray(L, t->node, sizenode(t), Node);
  luaM_freearray(L, t->array, t->sizearray, TObject);
  luaM_freelem(L, t);
}


#if 0
/*
** try to remove an element from a hash table; cannot move any element
** (because gc can call `remove' during a table traversal)
*/
void luaH_remove (Table *t, Node *e) {
  Node *mp = luaH_mainposition(t, key(e));
  if (e != mp) {  /* element not in its main position? */
    while (mp->next != e) mp = mp->next;  /* find previous */
    mp->next = e->next;  /* remove `e' from its list */
  }
  else {
    if (e->next != NULL) ??
  }
  lua_assert(ttype(val(node)) == LUA_TNIL);
  setnilvalue(key(e));  /* clear node `e' */
  e->next = NULL;
}
#endif


/*
** inserts a new key into a hash table; first, check whether key's main 
** position is free. If not, check whether colliding node is in its main 
** position or not: if it is not, move colliding node to an empty place and 
** put new key in its main position; otherwise (colliding node is in its main 
** position), new key goes to an empty position. 
*/
static void newkey (lua_State *L, Table *t, const TObject *key,
                                           const TObject *val) {
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
  settableval(val(mp), val);
  for (;;) {  /* correct `firstfree' */
    if (ttype(key(t->firstfree)) == LUA_TNIL)
      return;  /* OK; table still has a free place */
    else if (t->firstfree == t->node) break;  /* cannot decrement from here */
    else (t->firstfree)--;
  }
  rehash(L, t);  /* no more free places; must create one */
}


/*
** generic search function
*/
static const TObject *luaH_getany (Table *t, const TObject *key) {
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
const TObject *luaH_getnum (Table *t, int key) {
  if (1 <= key && key <= t->sizearray)
    return &t->array[key-1];
  else {
    Node *n = hashnum(t, key);
    do {  /* check whether `key' is somewhere in the chain */
      if (ttype(key(n)) == LUA_TNUMBER && nvalue(key(n)) == (lua_Number)key)
        return val(n);  /* that's it */
      else n = n->next;
    } while (n);
    return &luaO_nilobject;
  }
}


/*
** search function for strings
*/
const TObject *luaH_getstr (Table *t, TString *key) {
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
const TObject *luaH_get (Table *t, const TObject *key) {
  switch (ttype(key)) {
    case LUA_TSTRING: return luaH_getstr(t, tsvalue(key));
    case LUA_TNUMBER: {
      int k = cast(int, nvalue(key));
      if (cast(lua_Number, k) == nvalue(key))  /* is an integer index? */
        return luaH_getnum(t, k);  /* use specialized version */
      /* else go through */
    }
    default: return luaH_getany(t, key);
  }
}


void luaH_set (lua_State *L, Table *t, const TObject *key, const TObject *val) {
  const TObject *p = luaH_get(t, key);
  if (p != &luaO_nilobject) {
    settableval(p, val);
  }
  else {
    if (ttype(key) == LUA_TNIL) luaD_error(L, "table index is nil");
    newkey(L, t, key, val);
  }
  t->flags = 0;
}


void luaH_setnum (lua_State *L, Table *t, int key, const TObject *val) {
  const TObject *p = luaH_getnum(t, key);
  if (p != &luaO_nilobject) {
    settableval(p, val);
  }
  else {
    TObject k;
    setnvalue(&k, key);
    newkey(L, t, &k, val);
  }
}

