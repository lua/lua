/*
** $Id: ltable.c,v 1.24 1999/09/22 14:38:45 roberto Exp roberto $
** Lua tables (hash)
** See Copyright Notice in lua.h
*/


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



static long int hashindex (const TObject *ref) {
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


Node *luaH_present (const Hash *t, const TObject *key) {
  const int tsize = nhash(t);
  const long int h = hashindex(key);
  int h1 = h%tsize;
  Node *n = node(t, h1);
  /* keep looking until an entry with "ref" equal to key or nil */
  while ((ttype(ref(n)) == ttype(key)) ? !luaO_equalval(key, ref(n))
                                       : ttype(ref(n)) != LUA_T_NIL) {
    h1 += (h&(tsize-2)) + 1;  /* double hashing */
    if (h1 >= tsize) h1 -= tsize;
    n = node(t, h1);
  }
  return n;
}


static Node *hashnodecreate (int nhash) {
  Node *const v = luaM_newvector(nhash, Node);
  int i;
  for (i=0; i<nhash; i++)
    ttype(ref(&v[i])) = ttype(val(&v[i])) = LUA_T_NIL;
  return v;
}


Hash *luaH_new (int nhash) {
  Hash *const t = luaM_new(Hash);
  nhash = luaO_redimension(nhash*3/2);
  nodevector(t) = hashnodecreate(nhash);
  nhash(t) = nhash;
  nuse(t) = 0;
  t->htag = TagDefault;
  t->next = L->roottable;
  L->roottable = t;
  t->marked = 0;
  L->nblocks += gcsize(nhash);
  return t;
}


void luaH_free (Hash *t) {
  L->nblocks -= gcsize(t->nhash);
  luaM_free(nodevector(t));
  luaM_free(t);
}


static int newsize (Hash *t) {
  Node *const v = t->node;
  const int size = nhash(t);
  int realuse = 0;
  int i;
  for (i=0; i<size; i++) {
    if (ttype(val(v+i)) != LUA_T_NIL)
      realuse++;
  }
  return luaO_redimension(realuse*2);
}


static void rehash (Hash *t) {
  const int nold = nhash(t);
  Node *const vold = nodevector(t);
  const int nnew = newsize(t);
  int i;
  nodevector(t) = hashnodecreate(nnew);
  nhash(t) = nnew;
  nuse(t) = 0;
  for (i=0; i<nold; i++) {
    Node *n = vold+i;
    if (ttype(val(n)) != LUA_T_NIL) {
      *luaH_present(t, ref(n)) = *n;  /* copy old node to new hash */
      nuse(t)++;
    }
  }
  L->nblocks += gcsize(nnew)-gcsize(nold);
  luaM_free(vold);
}


void luaH_set (Hash *t, const TObject *ref, const TObject *val) {
  Node *const n = luaH_present(t, ref);
  *val(n) = *val;
  if (ttype(ref(n)) == LUA_T_NIL) {  /* new node? */
    *ref(n) = *ref;  /* set key */
    nuse(t)++;  /* count it */
    if ((long)nuse(t)*3L > (long)nhash(t)*2L)  /* check size */
      rehash(t);
  }
}


int luaH_pos (const Hash *t, const TObject *r) {
  Node *const n = luaH_present(t, r);
  luaL_arg_check(ttype(val(n)) != LUA_T_NIL, 2, "key not found");
  return n-(t->node);
}


void luaH_setint (Hash *t, int ref, const TObject *val) {
  TObject index;
  ttype(&index) = LUA_T_NUMBER;
  nvalue(&index) = ref;
  luaH_set(t, &index, val);
}


TObject *luaH_getint (const Hash *t, int ref) {
  TObject index;
  ttype(&index) = LUA_T_NUMBER;
  nvalue(&index) = ref;
  return luaH_get(t, &index);
}

