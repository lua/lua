/*
** $Id: ltable.c,v 1.16 1998/12/30 13:14:46 roberto Exp $
** Lua tables (hash)
** See Copyright Notice in lua.h
*/

#include <stdlib.h>

#include "lauxlib.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "ltable.h"
#include "lua.h"


#define gcsize(n)	(1+(n/16))

#define nuse(t)		((t)->nuse)
#define nodevector(t)	((t)->node)


#define TagDefault LUA_T_ARRAY;



static long int hashindex (TObject *ref)
{
  long int h;
  switch (ttype(ref)) {
    case LUA_T_NUMBER:
      h = (long int)nvalue(ref);
      break;
    case LUA_T_STRING: case LUA_T_USERDATA:
      h = (IntPoint)tsvalue(ref);
      break;
    case LUA_T_ARRAY:
      h = (IntPoint)avalue(ref);
      break;
    case LUA_T_PROTO:
      h = (IntPoint)tfvalue(ref);
      break;
    case LUA_T_CPROTO:
      h = (IntPoint)fvalue(ref);
      break;
    case LUA_T_CLOSURE:
      h = (IntPoint)clvalue(ref);
      break;
    default:
      lua_error("unexpected type to index table");
      h = 0;  /* to avoid warnings */
  }
  return (h >= 0 ? h : -(h+1));
}


static int present (Hash *t, TObject *key)
{
  int tsize = nhash(t);
  long int h = hashindex(key);
  int h1 = h%tsize;
  TObject *rf = ref(node(t, h1));
  if (ttype(rf) != LUA_T_NIL && !luaO_equalObj(key, rf)) {
    int h2 = h%(tsize-2) + 1;
    do {
      h1 += h2;
      if (h1 >= tsize) h1 -= tsize;
      rf = ref(node(t, h1));
    } while (ttype(rf) != LUA_T_NIL && !luaO_equalObj(key, rf));
  }
  return h1;
}


/*
** Alloc a vector node
*/
static Node *hashnodecreate (int nhash)
{
  Node *v = luaM_newvector(nhash, Node);
  int i;
  for (i=0; i<nhash; i++)
    ttype(ref(&v[i])) = LUA_T_NIL;
  return v;
}

/*
** Delete a hash
*/
static void hashdelete (Hash *t)
{
  luaM_free(nodevector(t));
  luaM_free(t);
}


void luaH_free (Hash *frees)
{
  while (frees) {
    Hash *next = (Hash *)frees->head.next;
    L->nblocks -= gcsize(frees->nhash);
    hashdelete(frees);
    frees = next;
  }
}


Hash *luaH_new (int nhash) {
  Hash *t = luaM_new(Hash);
  nhash = luaO_redimension(nhash*3/2);
  nodevector(t) = hashnodecreate(nhash);
  nhash(t) = nhash;
  nuse(t) = 0;
  t->htag = TagDefault;
  luaO_insertlist(&(L->roottable), (GCnode *)t);
  L->nblocks += gcsize(nhash);
  return t;
}


static int newsize (Hash *t) {
  Node *v = t->node;
  int size = nhash(t);
  int realuse = 0;
  int i;
  for (i=0; i<size; i++) {
    if (ttype(ref(v+i)) != LUA_T_NIL && ttype(val(v+i)) != LUA_T_NIL)
      realuse++;
  }
  return luaO_redimension((realuse+1)*2);  /* +1 is the new element */
}

static void rehash (Hash *t) {
  int nold = nhash(t);
  Node *vold = nodevector(t);
  int nnew = newsize(t);
  int i;
  nodevector(t) = hashnodecreate(nnew);
  nhash(t) = nnew;
  nuse(t) = 0;
  for (i=0; i<nold; i++) {
    Node *n = vold+i;
    if (ttype(ref(n)) != LUA_T_NIL && ttype(val(n)) != LUA_T_NIL) {
      *node(t, present(t, ref(n))) = *n;  /* copy old node to luaM_new hash */
      nuse(t)++;
    }
  }
  L->nblocks += gcsize(nnew)-gcsize(nold);
  luaM_free(vold);
}

/*
** If the hash node is present, return its pointer, otherwise return
** null.
*/
TObject *luaH_get (Hash *t, TObject *ref)
{
 int h = present(t, ref);
 if (ttype(ref(node(t, h))) != LUA_T_NIL) return val(node(t, h));
 else return &luaO_nilobject;
}


/*
** If the hash node is present, return its pointer, otherwise create a luaM_new
** node for the given reference and also return its pointer.
*/
TObject *luaH_set (Hash *t, TObject *ref)
{
  Node *n = node(t, present(t, ref));
  if (ttype(ref(n)) == LUA_T_NIL) {
    if ((long)nuse(t)*3L > (long)nhash(t)*2L) {
      rehash(t);
      n = node(t, present(t, ref));
    }
    nuse(t)++;
    *ref(n) = *ref;
    ttype(val(n)) = LUA_T_NIL;
  }
  return (val(n));
}


static Node *hashnext (Hash *t, int i) {
  Node *n;
  int tsize = nhash(t);
  if (i >= tsize)
    return NULL;
  n = node(t, i);
  while (ttype(ref(n)) == LUA_T_NIL || ttype(val(n)) == LUA_T_NIL) {
    if (++i >= tsize)
      return NULL;
    n = node(t, i);
  }
  return node(t, i);
}

Node *luaH_next (Hash *t, TObject *r) {
  if (ttype(r) == LUA_T_NIL)
    return hashnext(t, 0);
  else {
    int i = present(t, r);
    Node *n = node(t, i);
    luaL_arg_check(ttype(ref(n))!=LUA_T_NIL && ttype(val(n))!=LUA_T_NIL,
                   2, "key not found");
    return hashnext(t, i+1);
  }
}


void luaH_setint (Hash *t, int ref, TObject *val) {
  TObject index;
  ttype(&index) = LUA_T_NUMBER;
  nvalue(&index) = ref;
  *(luaH_set(t, &index)) = *val;
}


TObject *luaH_getint (Hash *t, int ref) {
  TObject index;
  ttype(&index) = LUA_T_NUMBER;
  nvalue(&index) = ref;
  return luaH_get(t, &index);
}


void luaH_move (Hash *t, int from, int to) {
  TObject index;
  TObject *toadd;
  ttype(&index) = LUA_T_NUMBER;
  nvalue(&index) = to;
  toadd = luaH_set(t, &index);
  nvalue(&index) = from;
  *toadd = *luaH_get(t, &index);
}
