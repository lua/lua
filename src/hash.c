/*
** hash.c
** hash manager for lua
*/

char *rcs_hash="$Id: hash.c,v 2.43 1997/05/14 18:38:29 roberto Exp $";


#include "luamem.h"
#include "opcode.h"
#include "hash.h"
#include "table.h"
#include "lua.h"
#include "auxlib.h"


#define nhash(t)	((t)->nhash)
#define nuse(t)		((t)->nuse)
#define markarray(t)    ((t)->mark)
#define nodevector(t)	((t)->node)
#define node(t,i)	(&(t)->node[i])
#define ref(n)		(&(n)->ref)
#define val(n)		(&(n)->val)


#define REHASH_LIMIT    0.70    /* avoid more than this % full */

#define TagDefault LUA_T_ARRAY;


static Hash *listhead = NULL;


/* hash dimensions values */
static Long dimensions[] = 
 {5L, 11L, 23L, 47L, 97L, 197L, 397L, 797L, 1597L, 3203L, 6421L,
  12853L, 25717L, 51437L, 102811L, 205619L, 411233L, 822433L,
  1644817L, 3289613L, 6579211L, 13158023L, MAX_INT};

int luaI_redimension (int nhash)
{
 int i;
 for (i=0; dimensions[i]<MAX_INT; i++)
 {
  if (dimensions[i] > nhash)
   return dimensions[i];
 }
 lua_error("table overflow");
 return 0;  /* to avoid warnings */
}


int lua_equalObj (TObject *t1, TObject *t2)
{
  if (ttype(t1) != ttype(t2)) return 0;
  switch (ttype(t1))
  {
    case LUA_T_NIL: return 1;
    case LUA_T_NUMBER: return nvalue(t1) == nvalue(t2);
    case LUA_T_STRING: case LUA_T_USERDATA: return svalue(t1) == svalue(t2);
    case LUA_T_ARRAY: return avalue(t1) == avalue(t2);
    case LUA_T_FUNCTION: return t1->value.tf == t2->value.tf;
    case LUA_T_CFUNCTION: return fvalue(t1) == fvalue(t2);
    default:
     lua_error("internal error in `lua_equalObj'");
     return 0; /* UNREACHEABLE */
  }
}


static long int hashindex (TObject *ref)
{
  long int h;
  switch (ttype(ref)) {
    case LUA_T_NUMBER:
      h = (long int)nvalue(ref); break;
    case LUA_T_STRING: case LUA_T_USERDATA:
      h = tsvalue(ref)->hash; break;
    case LUA_T_FUNCTION:
      h = (IntPoint)ref->value.tf; break;
    case LUA_T_CFUNCTION:
      h = (IntPoint)fvalue(ref); break;
    case LUA_T_ARRAY:
      h = (IntPoint)avalue(ref); break;
    default:
      lua_error ("unexpected type to index table");
      h = 0;  /* UNREACHEABLE */
  }
  if (h < 0) h = -h;
  return h;
}


static int present (Hash *t, TObject *key)
{
  long int h = hashindex(key);
  int tsize = nhash(t);
  int h1 = h%tsize;
  TObject *rf = ref(node(t, h1));
  if (ttype(rf) != LUA_T_NIL && !lua_equalObj(key, rf)) {
    int h2 = h%(tsize-2) + 1;
    do {
      h1 = (h1+h2)%tsize;
      rf = ref(node(t, h1));
    } while (ttype(rf) != LUA_T_NIL && !lua_equalObj(key, rf));
  }
  return h1;
}


/*
** Alloc a vector node 
*/
static Node *hashnodecreate (int nhash)
{
 int i;
 Node *v = newvector (nhash, Node);
 for (i=0; i<nhash; i++)
   ttype(ref(&v[i])) = LUA_T_NIL;
 return v;
}

/*
** Create a new hash. Return the hash pointer or NULL on error.
*/
static Hash *hashcreate (int nhash)
{
 Hash *t = new(Hash);
 nhash = luaI_redimension((int)((float)nhash/REHASH_LIMIT));
 nodevector(t) = hashnodecreate(nhash);
 nhash(t) = nhash;
 nuse(t) = 0;
 markarray(t) = 0;
 t->htag = TagDefault;
 return t;
}

/*
** Delete a hash
*/
static void hashdelete (Hash *t)
{
 luaI_free (nodevector(t));
 luaI_free(t);
}


/*
** Mark a hash and check its elements 
*/
void lua_hashmark (Hash *h)
{
 if (markarray(h) == 0)
 {
  int i;
  markarray(h) = 1;
  for (i=0; i<nhash(h); i++)
  {
   Node *n = node(h,i);
   if (ttype(ref(n)) != LUA_T_NIL)
   {
    lua_markobject(&n->ref);
    lua_markobject(&n->val);
   }
  }
 } 
}


void luaI_hashcallIM (Hash *l)
{
  TObject t;
  ttype(&t) = LUA_T_ARRAY;
  for (; l; l=l->next) {
    avalue(&t) = l;
    luaI_gcIM(&t);
  }
}


void luaI_hashfree (Hash *frees)
{
  while (frees) {
    Hash *next = frees->next;
    hashdelete(frees);
    frees = next;
  }
}

 
Hash *luaI_hashcollector (long *acum)
{
  Hash *curr_array = listhead, *prev = NULL, *frees = NULL;
  long counter = 0;
  while (curr_array != NULL) {
    Hash *next = curr_array->next;
    if (markarray(curr_array) != 1) {
      if (prev == NULL)
        listhead = next;
      else
        prev->next = next;
      curr_array->next = frees;
      frees = curr_array;
      ++counter;
    }
    else {
      markarray(curr_array) = 0;
      prev = curr_array;
    }
    curr_array = next;
  }
  *acum += counter;
  return frees;
}


/*
** Create a new array
** This function inserts the new array in the array list. It also
** executes garbage collection if the number of arrays created
** exceed a pre-defined range.
*/
Hash *lua_createarray (int nhash)
{
 Hash *array;
 lua_pack();
 array = hashcreate(nhash);
 array->next = listhead;
 listhead = array;
 return array;
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
    nhash(t) = luaI_redimension(nhash(t));
  nodevector(t) = hashnodecreate(nhash(t));
  for (i=0; i<nold; i++) {
    Node *n = vold+i;
    if (ttype(ref(n)) != LUA_T_NIL && ttype(val(n)) != LUA_T_NIL)
      *node(t, present(t, ref(n))) = *n;  /* copy old node to new hash */
  }
  luaI_free(vold);
}

/*
** If the hash node is present, return its pointer, otherwise return
** null.
*/
TObject *lua_hashget (Hash *t, TObject *ref)
{
 int h = present(t, ref);
 if (ttype(ref(node(t, h))) != LUA_T_NIL) return val(node(t, h));
 else return NULL;
}


/*
** If the hash node is present, return its pointer, otherwise create a new
** node for the given reference and also return its pointer.
*/
TObject *lua_hashdefine (Hash *t, TObject *ref)
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


/*
** Internal function to manipulate arrays. 
** Given an array object and a reference value, return the next element
** in the hash.
** This function pushs the element value and its reference to the stack.
*/
static void hashnext (Hash *t, int i)
{
  Node *n;
  int tsize = nhash(t);
  if (i >= tsize)
    return;
  n = node(t, i);
  while (ttype(ref(n)) == LUA_T_NIL || ttype(val(n)) == LUA_T_NIL) {
    if (++i >= tsize)
      return;
    n = node(t, i);
  }
  luaI_pushobject(ref(node(t,i)));
  luaI_pushobject(val(node(t,i)));
}

void lua_next (void)
{
  Hash *t;
  lua_Object o = lua_getparam(1);
  lua_Object r = lua_getparam(2);
  luaL_arg_check(lua_istable(o), 1, "table expected");
  luaL_arg_check(r != LUA_NOOBJECT, 2, "value expected");
  t = avalue(luaI_Address(o));
  if (lua_isnil(r))
    hashnext(t, 0);
  else
    hashnext(t, present(t, luaI_Address(r))+1);
}
