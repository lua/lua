/*
** $Id: ltable.c,v 1.33 1999/12/23 18:19:57 roberto Exp roberto $
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
#include "ltable.h"
#include "lua.h"


#define gcsize(L, n)	numblocks(L, n*2, sizeof(Hash))



#define TagDefault LUA_T_ARRAY;



/*
** returns the `main' position of an element in a table (that is, the index
** of its hash value)
*/
Node *luaH_mainposition (const Hash *t, const TObject *key) {
  unsigned long h;
  switch (ttype(key)) {
    case LUA_T_NUMBER:
      h = (unsigned long)(long)nvalue(key);
      break;
    case LUA_T_STRING: case LUA_T_USERDATA:
      h = tsvalue(key)->hash;
      break;
    case LUA_T_ARRAY:
      h = IntPoint(L, avalue(key));
      break;
    case LUA_T_LPROTO:
      h = IntPoint(L, tfvalue(key));
      break;
    case LUA_T_CPROTO:
      h = IntPoint(L, fvalue(key));
      break;
    case LUA_T_LCLOSURE:  case LUA_T_CCLOSURE:
      h = IntPoint(L, clvalue(key));
      break;
    default:
      return NULL;  /* invalid key */
  }
  LUA_ASSERT(L, h%(unsigned int)t->size == (h&((unsigned int)t->size-1)),
            "a&(x-1) == a%x, for x power of 2");
  return &t->node[h&((unsigned int)t->size-1)];
}


const TObject *luaH_get (lua_State *L, const Hash *t, const TObject *key) {
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


int luaH_pos (lua_State *L, const Hash *t, const TObject *key) {
  const TObject *v = luaH_get(L, t, key);
  return (v == &luaO_nilobject) ?  -1 :  /* key not found */
      (int)(((const char *)v - (const char *)(&t->node[0].val))/sizeof(Node));
}



static Node *hashnodecreate (lua_State *L, int nhash) {
  Node *v = luaM_newvector(L, nhash, Node);
  int i;
  for (i=0; i<nhash; i++) {
    ttype(&v[i].key) = ttype(&v[i].val) = LUA_T_NIL;
    v[i].next = NULL;
  }
  return v;
}


static void setnodevector (lua_State *L, Hash *t, int size) {
  t->node = hashnodecreate(L, size);
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


static int newsize (const Hash *t) {
  Node *v = t->node;
  int size = t->size;
  int realuse = 0;
  int i;
  for (i=0; i<size; i++) {
    if (ttype(&v[i].val) != LUA_T_NIL)
      realuse++;
  }
  return luaO_power2(realuse+realuse/4+1);
}


static void rehash (lua_State *L, Hash *t) {
  int oldsize = t->size;
  Node *nold = t->node;
  int i;
  L->nblocks -= gcsize(L, oldsize);
  setnodevector(L, t, newsize(t));  /* create new array of nodes */
  for (i=0; i<oldsize; i++) {
    Node *old = nold+i;
    if (ttype(&old->val) != LUA_T_NIL)
       luaH_set(L, t, &old->key, &old->val);
  }
  luaM_free(L, nold);  /* free old array */
}


/*
** sets a pair key-value in a hash table; first, check whether key is
** already present; if not, check whether key's main position is free;
** if not, check whether colliding node is in its main position or not;
** if it is not, move colliding node to an empty place and put new pair
** in its main position; otherwise (colliding node is in its main position),
** new pair goes to an empty position.
** Tricky point: the only place where an old element is moved is when
** we move the colliding node to an empty place; nevertheless, its old
** value is still in that position until we set the value for the new
** pair; therefore, even when `val' points to an element of this table
** (this happens when we use `luaH_move'), there is no problem.
*/
void luaH_set (lua_State *L, Hash *t, const TObject *key, const TObject *val) {
  Node *mp = luaH_mainposition(t, key);
  Node *n = mp;
  if (!mp)
    lua_error(L, "unexpected type to index table");
  do {  /* check whether `key' is somewhere in the chain */
    if (luaO_equalObj(key, &n->key)) {
      n->val = *val;  /* update value */
      return;  /* that's all */
    }
    else n = n->next;
  } while (n);
  /* `key' not found; must insert it */
  if (ttype(&mp->key) != LUA_T_NIL) {  /* main position is not free? */
    Node *othern;  /* main position of colliding node */
    n = t->firstfree;  /* get a free place */
    /* is colliding node out of its main position? (can only happens if
       its position if after "firstfree") */
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
  mp->val = *val;
  for (;;) {  /* check free places */
    if (ttype(&(t->firstfree)->key) == LUA_T_NIL)
      return;  /* OK; table still has a free place */
    else if (t->firstfree == t->node) break;  /* cannot decrement from here */
    else (t->firstfree)--;
  }
  rehash(L, t);  /* no more free places */
}


void luaH_setint (lua_State *L, Hash *t, int key, const TObject *val) {
  TObject index;
  ttype(&index) = LUA_T_NUMBER;
  nvalue(&index) = key;
  luaH_set(L, t, &index, val);
}


const TObject *luaH_getint (lua_State *L, const Hash *t, int key) {
  TObject index;
  ttype(&index) = LUA_T_NUMBER;
  nvalue(&index) = key;
  return luaH_get(L, t, &index);
}

