/*
** $Id: ltable.c,v 1.1 1997/09/16 19:25:59 roberto Exp roberto $
** Lua tables (hash)
** See Copyright Notice in lua.h
*/

#include <stdlib.h>

#include "lauxlib.h"
#include "lmem.h"
#include "lobject.h"
#include "ltable.h"
#include "lua.h"


#define nuse(t)		((t)->nuse)
#define nodevector(t)	((t)->node)
#define val(n)		(&(n)->val)


#define REHASH_LIMIT    0.70    /* avoid more than this % full */

#define TagDefault LUA_T_ARRAY;


GCnode luaH_root = {NULL, 0};



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
    case LUA_T_FUNCTION:
      h = (IntPoint)clvalue(ref);
      break;
    case LUA_T_CFUNCTION:
      h = (IntPoint)fvalue(ref);
      break;
    case LUA_T_ARRAY:
      h = (IntPoint)avalue(ref);
      break;
    default:
      lua_error("unexpected type to index table");
      h = 0;  /* UNREACHEABLE */
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
      h1 = (h1+h2)%tsize;
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
 int i;
 Node *v = luaM_newvector (nhash, Node);
 for (i=0; i<nhash; i++)
   ttype(ref(&v[i])) = LUA_T_NIL;
 return v;
}

/*
** Delete a hash
*/
static void hashdelete (Hash *t)
{
  luaM_free (nodevector(t));
  luaM_free(t);
}


void luaH_free (Hash *frees)
{
  while (frees) {
    Hash *next = (Hash *)frees->head.next;
    hashdelete(frees);
    frees = next;
  }
}


Hash *luaH_new (int nhash)
{
  Hash *t = luaM_new(Hash);
  nhash = luaO_redimension((int)((float)nhash/REHASH_LIMIT));
  nodevector(t) = hashnodecreate(nhash);
  nhash(t) = nhash;
  nuse(t) = 0;
  t->htag = TagDefault;
  luaO_insertlist(&luaH_root, (GCnode *)t);
  return t;
}


/*
** Rehash:
** Check if table has deleted slots. It it has, it does not need to
** grow, since rehash will reuse them.
*/
static int emptyslots (Hash *t)
{
  int i;
  for (i=nhash(t)-1; i>=0; i--) {
    Node *n = node(t, i);
    if (ttype(ref(n)) != LUA_T_NIL && ttype(val(n)) == LUA_T_NIL)
      return 1;
  }
  return 0;
}

static void rehash (Hash *t)
{
  int   nold = nhash(t);
  Node *vold = nodevector(t);
  int i;
  if (!emptyslots(t))
    nhash(t) = luaO_redimension(nhash(t));
  nodevector(t) = hashnodecreate(nhash(t));
  for (i=0; i<nold; i++) {
    Node *n = vold+i;
    if (ttype(ref(n)) != LUA_T_NIL && ttype(val(n)) != LUA_T_NIL)
      *node(t, present(t, ref(n))) = *n;  /* copy old node to luaM_new hash */
  }
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
 else return NULL;
}


/*
** If the hash node is present, return its pointer, otherwise create a luaM_new
** node for the given reference and also return its pointer.
*/
TObject *luaH_set (Hash *t, TObject *ref)
{
  Node *n = node(t, present(t, ref));
  if (ttype(ref(n)) == LUA_T_NIL) {
    nuse(t)++;
    if ((float)nuse(t) > (float)nhash(t)*REHASH_LIMIT) {
      rehash(t);
      n = node(t, present(t, ref));
    }
    *ref(n) = *ref;
    ttype(val(n)) = LUA_T_NIL;
  }
  return (val(n));
}


static Node *hashnext (Hash *t, int i)
{
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

Node *luaH_next (TObject *o, TObject *r)
{
  Hash *t = avalue(o);
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
