/*
** $Id: ltable.c,v 2.5 2004/08/31 17:57:33 roberto Exp $
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
** Hence even when the load factor reaches 100%, performance remains good.
*/

#include <string.h>

#define ltable_c
#define LUA_CORE

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "ltable.h"


/*
** max size of array part is 2^MAXBITS
*/
#if LUA_BITSINT > 26
#define MAXBITS		24
#else
#define MAXBITS		(LUA_BITSINT-2)
#endif

#define MAXASIZE	(1 << MAXBITS)


#define hashpow2(t,n)      (gnode(t, lmod((n), sizenode(t))))
  
#define hashstr(t,str)  hashpow2(t, (str)->tsv.hash)
#define hashboolean(t,p)        hashpow2(t, p)


/*
** for some types, it is better to avoid modulus by power of 2, as
** they tend to have many 2 factors.
*/
#define hashmod(t,n)	(gnode(t, ((n) % ((sizenode(t)-1)|1))))


#define hashpointer(t,p)	hashmod(t, IntPoint(p))


/*
** number of ints inside a lua_Number
*/
#define numints		cast(int, sizeof(lua_Number)/sizeof(int))


/*
** hash for lua_Numbers
*/
static Node *hashnum (const Table *t, lua_Number n) {
  unsigned int a[numints];
  int i;
  n += 1;  /* normalize number (avoid -0) */
  lua_assert(sizeof(a) <= sizeof(n));
  memcpy(a, &n, sizeof(a));
  for (i = 1; i < numints; i++) a[0] += a[i];
  return hashmod(t, a[0]);
}



/*
** returns the `main' position of an element in a table (that is, the index
** of its hash value)
*/
Node *luaH_mainposition (const Table *t, const TValue *key) {
  switch (ttype(key)) {
    case LUA_TNUMBER:
      return hashnum(t, nvalue(key));
    case LUA_TSTRING:
      return hashstr(t, rawtsvalue(key));
    case LUA_TBOOLEAN:
      return hashboolean(t, bvalue(key));
    case LUA_TLIGHTUSERDATA:
      return hashpointer(t, pvalue(key));
    default:
      return hashpointer(t, gcvalue(key));
  }
}


/*
** returns the index for `key' if `key' is an appropriate key to live in
** the array part of the table, -1 otherwise.
*/
static int arrayindex (const TValue *key, lua_Number lim) {
  if (ttisnumber(key)) {
    lua_Number n = nvalue(key);
    int k;
    if (n <= 0 || n > lim) return -1;  /* out of range? */
    lua_number2int(k, n);
    if (cast(lua_Number, k) == nvalue(key))
      return k;
  }
  return -1;  /* `key' did not match some condition */
}


/*
** returns the index of a `key' for table traversals. First goes all
** elements in the array part, then elements in the hash part. The
** beginning and end of a traversal are signalled by -1.
*/
static int luaH_index (lua_State *L, Table *t, StkId key) {
  int i;
  if (ttisnil(key)) return -1;  /* first iteration */
  i = arrayindex(key, t->sizearray);
  if (0 <= i) {  /* is `key' inside array part? */
    return i-1;  /* yes; that's the index (corrected to C) */
  }
  else {
    const TValue *v = luaH_get(t, key);
    if (v == &luaO_nilobject)
      luaG_runerror(L, "invalid key for `next'");
    i = cast(int, (cast(const lu_byte *, v) -
                   cast(const lu_byte *, gval(gnode(t, 0)))) / sizeof(Node));
    return i + t->sizearray;  /* hash elements are numbered after array ones */
  }
}


int luaH_next (lua_State *L, Table *t, StkId key) {
  int i = luaH_index(L, t, key);  /* find original element */
  for (i++; i < t->sizearray; i++) {  /* try first array part */
    if (!ttisnil(&t->array[i])) {  /* a non-nil value? */
      setnvalue(key, cast(lua_Number, i+1));
      setobj2s(L, key+1, &t->array[i]);
      return 1;
    }
  }
  for (i -= t->sizearray; i < sizenode(t); i++) {  /* then hash part */
    if (!ttisnil(gval(gnode(t, i)))) {  /* a non-nil value? */
      setobj2s(L, key, gkey(gnode(t, i)));
      setobj2s(L, key+1, gval(gnode(t, i)));
      return 1;
    }
  }
  return 0;  /* no more elements */
}


/*
** {=============================================================
** Rehash
** ==============================================================
*/


static void computesizes  (int nums[], int ntotal, int *narray, int *nhash) {
  int i;
  int a = nums[0];  /* number of elements smaller than 2^i */
  int na = a;  /* number of elements to go to array part */
  int n = (na == 0) ? -1 : 0;  /* (log of) optimal size for array part */
  for (i = 1; a < *narray && *narray >= twoto(i-1); i++) {
    if (nums[i] > 0) {
      a += nums[i];
      if (a >= twoto(i-1)) {  /* more than half elements in use? */
        n = i;
        na = a;
      }
    }
  }
  lua_assert(na <= *narray && *narray <= ntotal);
  *nhash = ntotal - na;
  *narray = (n == -1) ? 0 : twoto(n);
  lua_assert(na <= *narray && na >= *narray/2);
}


static void numuse (const Table *t, int *narray, int *nhash) {
  int nums[MAXBITS+1];
  int i, lg;
  int totaluse = 0;
  lua_Number sizelimit;  /* an upper bound for the array size */
  /* count elements in array part */
  for (i=0, lg=0; lg<=MAXBITS; lg++) {  /* for each slice [2^(lg-1) to 2^lg) */
    int ttlg = twoto(lg);  /* 2^lg */
    if (ttlg > t->sizearray) {
      ttlg = t->sizearray;
      if (i >= ttlg) break;
    }
    nums[lg] = 0;
    for (; i<ttlg; i++) {
      if (!ttisnil(&t->array[i])) {
        nums[lg]++;
        totaluse++;
      }
    }
  }
  for (; lg<=MAXBITS; lg++) nums[lg] = 0;  /* reset other counts */
  *narray = totaluse;  /* all previous uses were in array part */
  /* count elements in hash part */
  i = sizenode(t);
  /* array part cannot be larger than twice the maximum number of elements */
  sizelimit = cast(lua_Number, totaluse + i) * 2;
  if (sizelimit >= MAXASIZE) sizelimit = MAXASIZE;
  while (i--) {
    Node *n = &t->node[i];
    if (!ttisnil(gval(n))) {
      int k = arrayindex(gkey(n), sizelimit);
      if (k >= 0) {  /* is `key' an appropriate array index? */
        nums[luaO_log2(k-1)+1]++;  /* count as such */
        (*narray)++;
      }
      totaluse++;
    }
  }
  computesizes(nums, totaluse, narray, nhash);
}


static void setarrayvector (lua_State *L, Table *t, int size) {
  int i;
  luaM_reallocvector(L, t->array, t->sizearray, size, TValue);
  for (i=t->sizearray; i<size; i++)
     setnilvalue(&t->array[i]);
  t->sizearray = size;
}


static void setnodevector (lua_State *L, Table *t, int lsize) {
  int i;
  int size = twoto(lsize);
  if (lsize > MAXBITS)
    luaG_runerror(L, "table overflow");
  if (lsize == 0) {  /* no elements to hash part? */
    t->node = G(L)->dummynode;  /* use common `dummynode' */
    lua_assert(ttisnil(gkey(t->node)));  /* assert invariants: */
    lua_assert(ttisnil(gval(t->node)));
    lua_assert(t->node->next == NULL);  /* (`dummynode' must be empty) */
  }
  else {
    t->node = luaM_newvector(L, size, Node);
    for (i=0; i<size; i++) {
      t->node[i].next = NULL;
      setnilvalue(gkey(gnode(t, i)));
      setnilvalue(gval(gnode(t, i)));
    }
  }
  t->lsizenode = cast(lu_byte, lsize);
  t->firstfree = gnode(t, size-1);  /* first free position to be used */
}


void luaH_resize (lua_State *L, Table *t, int nasize, int nhsize) {
  int i;
  int oldasize = t->sizearray;
  int oldhsize = t->lsizenode;
  Node *nold;
  Node temp[1];
  if (oldhsize)
    nold = t->node;  /* save old hash ... */
  else {  /* old hash is `dummynode' */
    lua_assert(t->node == G(L)->dummynode);
    temp[0] = t->node[0];  /* copy it to `temp' */
    nold = temp;
    setnilvalue(gkey(G(L)->dummynode));  /* restate invariant */
    setnilvalue(gval(G(L)->dummynode));
    lua_assert(G(L)->dummynode->next == NULL);
  }
  if (nasize > oldasize)  /* array part must grow? */
    setarrayvector(L, t, nasize);
  /* create new hash part with appropriate size */
  setnodevector(L, t, nhsize);  
  /* re-insert elements */
  if (nasize < oldasize) {  /* array part must shrink? */
    t->sizearray = nasize;
    /* re-insert elements from vanishing slice */
    for (i=nasize; i<oldasize; i++) {
      if (!ttisnil(&t->array[i]))
        setobjt2t(L, luaH_setnum(L, t, i+1), &t->array[i]);
    }
    /* shrink array */
    luaM_reallocvector(L, t->array, oldasize, nasize, TValue);
  }
  /* re-insert elements in hash part */
  for (i = twoto(oldhsize) - 1; i >= 0; i--) {
    Node *old = nold+i;
    if (!ttisnil(gval(old)))
      setobjt2t(L, luaH_set(L, t, gkey(old)), gval(old));
  }
  if (oldhsize)
    luaM_freearray(L, nold, twoto(oldhsize), Node);  /* free old array */
}


static void rehash (lua_State *L, Table *t) {
  int nasize, nhsize;
  numuse(t, &nasize, &nhsize);  /* compute new sizes for array and hash parts */
  luaH_resize(L, t, nasize, luaO_log2(nhsize)+1);
}



/*
** }=============================================================
*/


Table *luaH_new (lua_State *L, int narray, int lnhash) {
  Table *t = luaM_new(L, Table);
  luaC_link(L, obj2gco(t), LUA_TTABLE);
  t->metatable = NULL;
  t->flags = cast(lu_byte, ~0);
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
  if (t->lsizenode)
    luaM_freearray(L, t->node, sizenode(t), Node);
  luaM_freearray(L, t->array, t->sizearray, TValue);
  luaM_freelem(L, t);
}



/*
** inserts a new key into a hash table; first, check whether key's main 
** position is free. If not, check whether colliding node is in its main 
** position or not: if it is not, move colliding node to an empty place and 
** put new key in its main position; otherwise (colliding node is in its main 
** position), new key goes to an empty position. 
*/
static TValue *newkey (lua_State *L, Table *t, const TValue *key) {
  TValue *val;
  Node *mp = luaH_mainposition(t, key);
  if (!ttisnil(gval(mp))) {  /* main position is not free? */
    Node *othern = luaH_mainposition(t, gkey(mp));  /* `mp' of colliding node */
    Node *n = t->firstfree;  /* get a free place */
    if (othern != mp) {  /* is colliding node out of its main position? */
      /* yes; move colliding node into free position */
      while (othern->next != mp) othern = othern->next;  /* find previous */
      othern->next = n;  /* redo the chain with `n' in place of `mp' */
      *n = *mp;  /* copy colliding node into free pos. (mp->next also goes) */
      mp->next = NULL;  /* now `mp' is free */
      setnilvalue(gval(mp));
    }
    else {  /* colliding node is in its own main position */
      /* new node will go into free position */
      n->next = mp->next;  /* chain new position */
      mp->next = n;
      mp = n;
    }
  }
  setobj2t(L, gkey(mp), key);
  luaC_barriert(L, t, key);
  lua_assert(ttisnil(gval(mp)));
  for (;;) {  /* correct `firstfree' */
    if (ttisnil(gkey(t->firstfree)))
      return gval(mp);  /* OK; table still has a free place */
    else if (t->firstfree == t->node) break;  /* cannot decrement from here */
    else (t->firstfree)--;
  }
  /* no more free places; must create one */
  setbvalue(gval(mp), 0);  /* avoid new key being removed */
  rehash(L, t);  /* grow table */
  val = cast(TValue *, luaH_get(t, key));  /* get new position */
  lua_assert(ttisboolean(val));
  setnilvalue(val);
  return val;
}


/*
** search function for integers
*/
const TValue *luaH_getnum (Table *t, int key) {
  if (1 <= key && key <= t->sizearray)
    return &t->array[key-1];
  else {
    lua_Number nk = cast(lua_Number, key);
    Node *n = hashnum(t, nk);
    do {  /* check whether `key' is somewhere in the chain */
      if (ttisnumber(gkey(n)) && nvalue(gkey(n)) == nk)
        return gval(n);  /* that's it */
      else n = n->next;
    } while (n);
    return &luaO_nilobject;
  }
}


/*
** search function for strings
*/
const TValue *luaH_getstr (Table *t, TString *key) {
  Node *n = hashstr(t, key);
  do {  /* check whether `key' is somewhere in the chain */
    if (ttisstring(gkey(n)) && rawtsvalue(gkey(n)) == key)
      return gval(n);  /* that's it */
    else n = n->next;
  } while (n);
  return &luaO_nilobject;
}


/*
** main search function
*/
const TValue *luaH_get (Table *t, const TValue *key) {
  switch (ttype(key)) {
    case LUA_TNIL: return &luaO_nilobject;
    case LUA_TSTRING: return luaH_getstr(t, rawtsvalue(key));
    case LUA_TNUMBER: {
      int k;
      lua_number2int(k, (nvalue(key)));
      if (cast(lua_Number, k) == nvalue(key))  /* is an integer index? */
        return luaH_getnum(t, k);  /* use specialized version */
      /* else go through */
    }
    default: {
      Node *n = luaH_mainposition(t, key);
      do {  /* check whether `key' is somewhere in the chain */
        if (luaO_rawequalObj(gkey(n), key)) return gval(n);  /* that's it */
        else n = n->next;
      } while (n);
      return &luaO_nilobject;
    }
  }
}


TValue *luaH_set (lua_State *L, Table *t, const TValue *key) {
  const TValue *p = luaH_get(t, key);
  t->flags = 0;
  if (p != &luaO_nilobject)
    return cast(TValue *, p);
  else {
    if (ttisnil(key)) luaG_runerror(L, "table index is nil");
    else if (ttisnumber(key) && nvalue(key) != nvalue(key))
      luaG_runerror(L, "table index is NaN");
    return newkey(L, t, key);
  }
}


TValue *luaH_setnum (lua_State *L, Table *t, int key) {
  const TValue *p = luaH_getnum(t, key);
  if (p != &luaO_nilobject)
    return cast(TValue *, p);
  else {
    TValue k;
    setnvalue(&k, cast(lua_Number, key));
    return newkey(L, t, &k);
  }
}


TValue *luaH_setstr (lua_State *L, Table *t, TString *key) {
  const TValue *p = luaH_getstr(t, key);
  if (p != &luaO_nilobject)
    return cast(TValue *, p);
  else {
    TValue k;
    setsvalue(L, &k, key);
    return newkey(L, t, &k);
  }
}

