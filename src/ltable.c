/*
** $Id: ltable.c,v 1.22 1999/05/21 19:41:49 roberto Exp $
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



static long int hashindex (TObject *ref) {
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


Node *luaH_present (Hash *t, TObject *key) {
  int tsize = nhash(t);
  long int h = hashindex(key);
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


void luaH_free (Hash *frees) {
  while (frees) {
    Hash *next = (Hash *)frees->head.next;
    L->nblocks -= gcsize(frees->nhash);
    luaM_free(nodevector(frees));
    luaM_free(frees);
    frees = next;
  }
}


static Node *hashnodecreate (int nhash) {
  Node *v = luaM_newvector(nhash, Node);
  int i;
  for (i=0; i<nhash; i++)
    ttype(ref(&v[i])) = ttype(val(&v[i])) = LUA_T_NIL;
  return v;
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
    if (ttype(val(v+i)) != LUA_T_NIL)
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
    if (ttype(val(n)) != LUA_T_NIL) {
      *luaH_present(t, ref(n)) = *n;  /* copy old node to new hash */
      nuse(t)++;
    }
  }
  L->nblocks += gcsize(nnew)-gcsize(nold);
  luaM_free(vold);
}


void luaH_set (Hash *t, TObject *ref, TObject *val) {
  Node *n = luaH_present(t, ref);
  if (ttype(ref(n)) != LUA_T_NIL)
    *val(n) = *val;
  else {
    TObject buff;
    buff = *val;  /* rehash may invalidate this address */
    if ((long)nuse(t)*3L > (long)nhash(t)*2L) {
      rehash(t);
      n = luaH_present(t, ref);
    }
    nuse(t)++;
    *ref(n) = *ref;
    *val(n) = buff;
  }
}


int luaH_pos (Hash *t, TObject *r) {
  Node *n = luaH_present(t, r);
  luaL_arg_check(ttype(val(n)) != LUA_T_NIL, 2, "key not found");
  return n-(t->node);
}


void luaH_setint (Hash *t, int ref, TObject *val) {
  TObject index;
  ttype(&index) = LUA_T_NUMBER;
  nvalue(&index) = ref;
  luaH_set(t, &index, val);
}


TObject *luaH_getint (Hash *t, int ref) {
  TObject index;
  ttype(&index) = LUA_T_NUMBER;
  nvalue(&index) = ref;
  return luaH_get(t, &index);
}

