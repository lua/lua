/*
** $Id: ltable.c,v 1.63 2001/01/10 18:56:11 roberto Exp roberto $
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


#include "lua.h"

#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "ltable.h"



#define TagDefault LUA_TTABLE


#define hashnum(t,n)	(&t->node[(luint32)(lint32)(n)&(t->size-1)])
#define hashstr(t,str)	(&t->node[(str)->u.s.hash&(t->size-1)])


/*
** returns the `main' position of an element in a table (that is, the index
** of its hash value)
*/
Node *luaH_mainposition (const Hash *t, const TObject *key) {
  luint32 h;
  switch (ttype(key)) {
    case LUA_TNUMBER:
      return hashnum(t, nvalue(key));
    case LUA_TSTRING:
      return hashstr(t, tsvalue(key));
    case LUA_TUSERDATA:
      h = IntPoint(tsvalue(key));
      break;
    case LUA_TTABLE:
      h = IntPoint(hvalue(key));
      break;
    case LUA_TFUNCTION:
      h = IntPoint(clvalue(key));
      break;
    default:
      return NULL;  /* invalid key */
  }
  return &t->node[h&(t->size-1)];
}


static const TObject *luaH_getany (const Hash *t, const TObject *key) {
  Node *n = luaH_mainposition(t, key);
  while (n) {
    if (luaO_equalObj(key, &n->key))
      return &n->val;
    n = n->next;
  }
  return &luaO_nilobject;  /* key not found */
}


/* specialized version for numbers */
const TObject *luaH_getnum (const Hash *t, lua_Number key) {
  Node *n = hashnum(t, key);
  do {
    if (nvalue(&n->key) == key && ttype(&n->key) == LUA_TNUMBER)
      return &n->val;
    n = n->next;
  } while (n);
  return &luaO_nilobject;  /* key not found */
}


/* specialized version for strings */
const TObject *luaH_getstr (const Hash *t, TString *key) {
  Node *n = hashstr(t, key);
  do {
    if (tsvalue(&n->key) == key && ttype(&n->key) == LUA_TSTRING)
      return &n->val;
    n = n->next;
  } while (n);
  return &luaO_nilobject;  /* key not found */
}


const TObject *luaH_get (const Hash *t, const TObject *key) {
  switch (ttype(key)) {
    case LUA_TNUMBER: return luaH_getnum(t, nvalue(key));
    case LUA_TSTRING: return luaH_getstr(t, tsvalue(key));
    default:         return luaH_getany(t, key);
  }
}


Node *luaH_next (lua_State *L, const Hash *t, const TObject *key) {
  int i;
  if (ttype(key) == LUA_TNIL)
    i = 0;  /* first iteration */
  else {
    const TObject *v = luaH_get(t, key);
    if (v == &luaO_nilobject)
      lua_error(L, "invalid key for `next'");
    i = (int)(((const char *)v -
               (const char *)(&t->node[0].val)) / sizeof(Node)) + 1;
  }
  for (; i<t->size; i++) {
    Node *n = node(t, i);
    if (ttype(val(n)) != LUA_TNIL)
      return n;
  }
  return NULL;  /* no more elements */
}


/*
** try to remove a key without value from a table. To avoid problems with
** hash, change `key' for a number with the same hash.
*/
void luaH_remove (Hash *t, TObject *key) {
  if (ttype(key) == LUA_TNUMBER ||
       (ttype(key) == LUA_TSTRING && tsvalue(key)->len <= 30))
  return;  /* do not remove numbers nor small strings */
  else {
    /* try to find a number `n' with the same hash as `key' */
    Node *mp = luaH_mainposition(t, key);
    int n = mp - &t->node[0];
    /* make sure `n' is not in `t' */
    while (luaH_getnum(t, n) != &luaO_nilobject) {
      if (n >= MAX_INT - t->size)
        return;  /* give up; (to avoid overflow) */
      n += t->size;
    }
    setnvalue(key, n);
    LUA_ASSERT(luaH_mainposition(t, key) == mp, "cannot change hash");
  }
}


static void setnodevector (lua_State *L, Hash *t, luint32 size) {
  int i;
  if (size > MAX_INT)
    lua_error(L, "table overflow");
  t->node = luaM_newvector(L, size, Node);
  for (i=0; i<(int)size; i++) {
    setnilvalue(&t->node[i].key);
    setnilvalue(&t->node[i].val);
    t->node[i].next = NULL;
  }
  t->size = size;
  t->firstfree = &t->node[size-1];  /* first free position to be used */
}


Hash *luaH_new (lua_State *L, int size) {
  Hash *t = luaM_new(L, Hash);
  t->htag = TagDefault;
  t->next = L->roottable;
  L->roottable = t;
  t->mark = t;
  t->size = 0;
  t->node = NULL;
  setnodevector(L, t, luaO_power2(size));
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
  LUA_ASSERT(nelems<=oldsize, "wrong count");
  if (nelems >= oldsize-oldsize/4)  /* using more than 3/4? */
    setnodevector(L, t, (luint32)oldsize*2);
  else if (nelems <= oldsize/4 &&  /* less than 1/4? */
           oldsize > MINPOWER2)
    setnodevector(L, t, oldsize/2);
  else
    setnodevector(L, t, oldsize);
  for (i=0; i<oldsize; i++) {
    Node *old = nold+i;
    if (ttype(&old->val) != LUA_TNIL)
      setobj(luaH_set(L, t, &old->key), &old->val);
  }
  luaM_freearray(L, nold, oldsize, Node);  /* free old array */
}


/*
** inserts a new key into a hash table; first, check whether key's main 
** position is free; if not, check whether colliding node is in its main 
** position or not; if it is not, move colliding node to an empty place and 
** put new key in its main position; otherwise (colliding node is in its main 
** position), new key goes to an empty position. 
*/
static TObject *newkey (lua_State *L, Hash *t, Node *mp, const TObject *key) {
  if (ttype(&mp->key) != LUA_TNIL) {  /* main position is not free? */
    Node *othern = luaH_mainposition(t, &mp->key);  /* `mp' of colliding node */
    Node *n = t->firstfree;  /* get a free place */
    if (othern != mp) {  /* is colliding node out of its main position? */
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
  setobj(&mp->key, key);
  for (;;) {  /* correct `firstfree' */
    if (ttype(&t->firstfree->key) == LUA_TNIL)
      return &mp->val;  /* OK; table still has a free place */
    else if (t->firstfree == t->node) break;  /* cannot decrement from here */
    else (t->firstfree)--;
  }
  rehash(L, t);  /* no more free places */
  return luaH_set(L, t, key);  /* `rehash' invalidates this insertion */
}


static TObject *luaH_setany (lua_State *L, Hash *t, const TObject *key) {
  Node *mp = luaH_mainposition(t, key);
  Node *n = mp;
  if (!mp)
    lua_error(L, "table index is nil");
  do {  /* check whether `key' is somewhere in the chain */
    if (luaO_equalObj(key, &n->key))
      return &n->val;  /* that's all */
    else n = n->next;
  } while (n);
  return newkey(L, t, mp, key);  /* `key' not found; must insert it */
}


TObject *luaH_setnum (lua_State *L, Hash *t, lua_Number key) {
  TObject kobj;
  Node *mp = hashnum(t, key);
  Node *n = mp;
  do {  /* check whether `key' is somewhere in the chain */
    if (nvalue(&n->key) == key && ttype(&n->key) == LUA_TNUMBER)
      return &n->val;  /* that's all */
    else n = n->next;
  } while (n);
  /* `key' not found; must insert it */
  setnvalue(&kobj, key);
  return newkey(L, t, mp, &kobj);
}


TObject *luaH_setstr (lua_State *L, Hash *t, TString *key) {
  TObject kobj;
  Node *mp = hashstr(t, key);
  Node *n = mp;
  do {  /* check whether `key' is somewhere in the chain */
    if (tsvalue(&n->key) == key && ttype(&n->key) == LUA_TSTRING)
      return &n->val;  /* that's all */
    else n = n->next;
  } while (n);
  /* `key' not found; must insert it */
  setsvalue(&kobj, key);
  return newkey(L, t, mp, &kobj);
}


TObject *luaH_set (lua_State *L, Hash *t, const TObject *key) {
  switch (ttype(key)) {
    case LUA_TNUMBER: return luaH_setnum(L, t, nvalue(key));
    case LUA_TSTRING: return luaH_setstr(L, t, tsvalue(key));
    default:         return luaH_setany(L, t, key);
  }
}

