/*
** $Id: ltable.c,v 1.45 2000/06/05 20:15:33 roberto Exp roberto $
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


#define LUA_REENTRANT

#include "lauxlib.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "lua.h"


#define gcsize(L, n)	numblocks(L, n*2, sizeof(Hash))



#define TagDefault TAG_TABLE



/*
** returns the `main' position of an element in a table (that is, the index
** of its hash value)
*/
Node *luaH_mainposition (const Hash *t, const TObject *key) {
  unsigned long h;
  switch (ttype(key)) {
    case TAG_NUMBER:
      h = (unsigned long)(long)nvalue(key);
      break;
    case TAG_STRING:
      h = tsvalue(key)->u.s.hash;
      break;
    case TAG_USERDATA:
      h = IntPoint(tsvalue(key));
      break;
    case TAG_TABLE:
      h = IntPoint(avalue(key));
      break;
    case TAG_LCLOSURE:  case TAG_CCLOSURE:
      h = IntPoint(clvalue(key));
      break;
    default:
      return NULL;  /* invalid key */
  }
  LUA_ASSERT(L, h%(unsigned int)t->size == (h&((unsigned int)t->size-1)),
            "a&(x-1) == a%x, for x power of 2");
  return &t->node[h&(t->size-1)];
}


static const TObject *luaH_getany (lua_State *L, const Hash *t,
                                   const TObject *key) {
  Node *n = luaH_mainposition(t, key);
  if (!n)
    lua_error(L, "unexpected type to index table");
  else do {
    if (luaO_equalObj(key, &n->key))
      return &n->val;
    n = n->next;
  } while (n);
  return &luaO_nilobject;  /* key not found */
}


/* specialized version for numbers */
const TObject *luaH_getnum (const Hash *t, Number key) {
  Node *n = &t->node[(unsigned long)(long)key&(t->size-1)];
  do {
    if (ttype(&n->key) == TAG_NUMBER && nvalue(&n->key) == key)
      return &n->val;
    n = n->next;
  } while (n);
  return &luaO_nilobject;  /* key not found */
}


/* specialized version for strings */
const TObject *luaH_getstr (const Hash *t, TString *key) {
  Node *n = &t->node[key->u.s.hash&(t->size-1)];
  do {
    if (ttype(&n->key) == TAG_STRING && tsvalue(&n->key) == key)
      return &n->val;
    n = n->next;
  } while (n);
  return &luaO_nilobject;  /* key not found */
}


const TObject *luaH_get (lua_State *L, const Hash *t, const TObject *key) {
  switch (ttype(key)) {
    case TAG_NUMBER: return luaH_getnum(t, nvalue(key));
    case TAG_STRING: return luaH_getstr(t, tsvalue(key));
    default:         return luaH_getany(L, t, key);
  }
}


int luaH_pos (lua_State *L, const Hash *t, const TObject *key) {
  const TObject *v = luaH_get(L, t, key);
  return (v == &luaO_nilobject) ?  -1 :  /* key not found */
      (int)(((const char *)v - (const char *)(&t->node[0].val))/sizeof(Node));
}

/*
** try to remove a key without value from a table. To avoid problems with
** hash, change `key' for a number with the same hash.
*/
void luaH_remove (Hash *t, TObject *key) {
  /* do not remove numbers */
  if (ttype(key) != TAG_NUMBER) {
    /* try to find a number `n' with the same hash as `key' */
    Node *mp = luaH_mainposition(t, key);
    int n = mp - &t->node[0];
    /* make sure `n' is not in `t' */
    while (luaH_getnum(t, n) != &luaO_nilobject) {
      if (t->size >= MAX_INT-n)
        return;  /* give up; (to avoid overflow) */
      n += t->size;
    }
    ttype(key) = TAG_NUMBER;
    nvalue(key) = n;
    LUA_ASSERT(L, luaH_mainposition(t, key) == mp, "cannot change hash");
  }
}


static void setnodevector (lua_State *L, Hash *t, lint32 size) {
  int i;
  if (size > MAX_INT)
    lua_error(L, "table overflow");
  t->node = luaM_newvector(L, size, Node);
  for (i=0; i<(int)size; i++) {
    ttype(&t->node[i].key) = ttype(&t->node[i].val) = TAG_NIL;
    t->node[i].next = NULL;
  }
  t->size = size;
  t->firstfree = &t->node[size-1];  /* first free position to be used */
  L->nblocks += gcsize(L, size);
}


Hash *luaH_new (lua_State *L, int size) {
  Hash *t = luaM_new(L, Hash);
  setnodevector(L, t, luaO_power2(size));
  t->htag = TagDefault;
  t->next = L->roottable;
  L->roottable = t;
  t->marked = 0;
  return t;
}


void luaH_free (lua_State *L, Hash *t) {
  L->nblocks -= gcsize(L, t->size);
  luaM_free(L, t->node);
  luaM_free(L, t);
}


static int numuse (const Hash *t) {
  Node *v = t->node;
  int size = t->size;
  int realuse = 0;
  int i;
  for (i=0; i<size; i++) {
    if (ttype(&v[i].val) != TAG_NIL)
      realuse++;
  }
  return realuse;
}


static void rehash (lua_State *L, Hash *t) {
  int oldsize = t->size;
  Node *nold = t->node;
  int nelems = numuse(t);
  int i;
  LUA_ASSERT(L, nelems<=oldsize, "wrong count");
  if (nelems >= oldsize-oldsize/4)  /* using more than 3/4? */
    setnodevector(L, t, (lint32)oldsize*2);
  else if (nelems <= oldsize/4 &&  /* less than 1/4? */
           oldsize > MINPOWER2)
    setnodevector(L, t, oldsize/2);
  else
    setnodevector(L, t, oldsize);
  L->nblocks -= gcsize(L, oldsize);
  for (i=0; i<oldsize; i++) {
    Node *old = nold+i;
    if (ttype(&old->val) != TAG_NIL)
      *luaH_set(L, t, &old->key) = old->val;
  }
  luaM_free(L, nold);  /* free old array */
}


/*
** inserts a key into a hash table; first, check whether key is
** already present; if not, check whether key's main position is free;
** if not, check whether colliding node is in its main position or not;
** if it is not, move colliding node to an empty place and put new key
** in its main position; otherwise (colliding node is in its main position),
** new key goes to an empty position.
*/
TObject *luaH_set (lua_State *L, Hash *t, const TObject *key) {
  Node *mp = luaH_mainposition(t, key);
  Node *n = mp;
  if (!mp)
    lua_error(L, "unexpected type to index table");
  do {  /* check whether `key' is somewhere in the chain */
    if (luaO_equalObj(key, &n->key))
      return &n->val;  /* that's all */
    else n = n->next;
  } while (n);
  /* `key' not found; must insert it */
  if (ttype(&mp->key) != TAG_NIL) {  /* main position is not free? */
    Node *othern;  /* main position of colliding node */
    n = t->firstfree;  /* get a free place */
    /* is colliding node out of its main position? (can only happens if
       its position is after "firstfree") */
    if (mp > n && (othern=luaH_mainposition(t, &mp->key)) != mp) {
      /* yes; move colliding node into free position */
      while (othern->next != mp) othern = othern->next;  /* find previous */
      othern->next = n;  /* redo the chain with `n' in place of `mp' */
      *n = *mp;  /* copy colliding node into free pos. (mp->next also goes) */
      mp->next = NULL;  /* now `mp' is free */
    }
    else {  /* colliding node is in its own main position */
      /* new node will go into free position */
      n->next = mp->next;  /* chain new position */
      mp->next = n;
      mp = n;
    }
  }
  mp->key = *key;
  for (;;) {  /* correct `firstfree' */
    if (ttype(&t->firstfree->key) == TAG_NIL)
      return &mp->val;  /* OK; table still has a free place */
    else if (t->firstfree == t->node) break;  /* cannot decrement from here */
    else (t->firstfree)--;
  }
  rehash(L, t);  /* no more free places */
  return luaH_set(L, t, key);  /* `rehash' invalidates this insertion */
}


TObject *luaH_setint (lua_State *L, Hash *t, int key) {
  TObject index;
  ttype(&index) = TAG_NUMBER;
  nvalue(&index) = key;
  return luaH_set(L, t, &index);
}


void luaH_setstrnum (lua_State *L, Hash *t, TString *key, Number val) {
  TObject *value, index;
  ttype(&index) = TAG_STRING;
  tsvalue(&index) = key;
  value = luaH_set(L, t, &index);
  ttype(value) = TAG_NUMBER;
  nvalue(value) = val;
}


const TObject *luaH_getglobal (lua_State *L, const char *name) {
  return luaH_getstr(L->gt, luaS_new(L, name));
}

