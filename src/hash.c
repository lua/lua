/*
** hash.c
** hash manager for lua
*/

char *rcs_hash="$Id: hash.c,v 2.30 1996/05/06 14:30:27 roberto Exp $";


#include "mem.h"
#include "opcode.h"
#include "hash.h"
#include "table.h"
#include "lua.h"


#define nhash(t)	((t)->nhash)
#define nuse(t)		((t)->nuse)
#define markarray(t)    ((t)->mark)
#define nodevector(t)	((t)->node)
#define node(t,i)	(&(t)->node[i])
#define ref(n)		(&(n)->ref)
#define val(n)		(&(n)->val)


#define REHASH_LIMIT    0.70    /* avoid more than this % full */


static Hash *listhead = NULL;


/* hash dimensions values */
static Long dimensions[] = 
 {3L, 5L, 7L, 11L, 23L, 47L, 97L, 197L, 397L, 797L, 1597L, 3203L, 6421L,
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

static int hashindex (Hash *t, Object *ref)		/* hash function */
{
 switch (tag(ref))
 {
  case LUA_T_NIL:
   lua_error ("unexpected type to index table");
   return -1;  /* UNREACHEABLE */
  case LUA_T_NUMBER:
   return (((int)nvalue(ref))%nhash(t));
  case LUA_T_STRING:
   return (int)((tsvalue(ref)->hash)%nhash(t));  /* make it a valid index */
  case LUA_T_FUNCTION:
   return (((IntPoint)ref->value.tf)%nhash(t));
  case LUA_T_CFUNCTION:
   return (((IntPoint)fvalue(ref))%nhash(t));
  case LUA_T_ARRAY:
   return (((IntPoint)avalue(ref))%nhash(t));
  default:  /* user data */
   return (((IntPoint)uvalue(ref))%nhash(t));
 }
}

int lua_equalObj (Object *t1, Object *t2)
{
  if (tag(t1) != tag(t2)) return 0;
  switch (tag(t1))
  {
    case LUA_T_NIL: return 1;
    case LUA_T_NUMBER: return nvalue(t1) == nvalue(t2);
    case LUA_T_STRING: return svalue(t1) == svalue(t2);
    case LUA_T_ARRAY: return avalue(t1) == avalue(t2);
    case LUA_T_FUNCTION: return t1->value.tf == t2->value.tf;
    case LUA_T_CFUNCTION: return fvalue(t1) == fvalue(t2);
    default: return uvalue(t1) == uvalue(t2);
  }
}

static int present (Hash *t, Object *ref)
{ 
 int h = hashindex(t, ref);
 while (tag(ref(node(t, h))) != LUA_T_NIL)
 {
  if (lua_equalObj(ref, ref(node(t, h))))
    return h;
  h = (h+1) % nhash(t);
 }
 return h;
}


/*
** Alloc a vector node 
*/
static Node *hashnodecreate (int nhash)
{
 int i;
 Node *v = newvector (nhash, Node);
 for (i=0; i<nhash; i++)
   tag(ref(&v[i])) = LUA_T_NIL;
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
   if (tag(ref(n)) != LUA_T_NIL)
   {
    lua_markobject(&n->ref);
    lua_markobject(&n->val);
   }
  }
 } 
}


static void call_fallbacks (void)
{
  Hash *curr_array;
  Object t;
  tag(&t) = LUA_T_ARRAY;
  for (curr_array = listhead; curr_array; curr_array = curr_array->next)
    if (markarray(curr_array) != 1)
    {
      avalue(&t) = curr_array;
      luaI_gcFB(&t);
    }
  tag(&t) = LUA_T_NIL;
  luaI_gcFB(&t);  /* end of list */
}

 
/*
** Garbage collection to arrays
** Delete all unmarked arrays.
*/
Long lua_hashcollector (void)
{
 Hash *curr_array = listhead, *prev = NULL;
 Long counter = 0;
 call_fallbacks();
 while (curr_array != NULL)
 {
  Hash *next = curr_array->next;
  if (markarray(curr_array) != 1)
  {
   if (prev == NULL) listhead = next;
   else              prev->next = next;
   hashdelete(curr_array);
   ++counter;
  }
  else
  {
   markarray(curr_array) = 0;
   prev = curr_array;
  }
  curr_array = next;
 }
 return counter;
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
** Re-hash
*/
static void rehash (Hash *t)
{
 int i;
 int   nold = nhash(t);
 Node *vold = nodevector(t);
 nhash(t) = luaI_redimension(nhash(t));
 nodevector(t) = hashnodecreate(nhash(t));
 for (i=0; i<nold; i++)
 {
  Node *n = vold+i;
  if (tag(ref(n)) != LUA_T_NIL && tag(val(n)) != LUA_T_NIL)
   *node(t, present(t, ref(n))) = *n;  /* copy old node to new hahs */
 }
 luaI_free(vold);
}

/*
** If the hash node is present, return its pointer, otherwise return
** null.
*/
Object *lua_hashget (Hash *t, Object *ref)
{
 int h = present(t, ref);
 if (tag(ref(node(t, h))) != LUA_T_NIL) return val(node(t, h));
 else return NULL;
}


/*
** If the hash node is present, return its pointer, otherwise create a new
** node for the given reference and also return its pointer.
*/
Object *lua_hashdefine (Hash *t, Object *ref)
{
 int   h;
 Node *n;
 h = present(t, ref);
 n = node(t, h);
 if (tag(ref(n)) == LUA_T_NIL)
 {
  nuse(t)++;
  if ((float)nuse(t) > (float)nhash(t)*REHASH_LIMIT)
  {
   rehash(t);
   h = present(t, ref);
   n = node(t, h);
  }
  *ref(n) = *ref;
  tag(val(n)) = LUA_T_NIL;
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
 if (i >= nhash(t))
 {
  lua_pushnil(); lua_pushnil();
  return;
 }
 while (tag(ref(node(t,i))) == LUA_T_NIL || tag(val(node(t,i))) == LUA_T_NIL)
 {
  if (++i >= nhash(t))
  {
   lua_pushnil(); lua_pushnil();
   return;
  }
 }
 luaI_pushobject(ref(node(t,i)));
 luaI_pushobject(val(node(t,i)));
}

void lua_next (void)
{
 Hash   *t;
 lua_Object o = lua_getparam(1);
 lua_Object r = lua_getparam(2);
 if (o == LUA_NOOBJECT || r == LUA_NOOBJECT)
   lua_error ("too few arguments to function `next'");
 if (lua_getparam(3) != LUA_NOOBJECT)
   lua_error ("too many arguments to function `next'");
 if (!lua_istable(o))
   lua_error ("first argument of function `next' is not a table");
 t = avalue(luaI_Address(o));
 if (lua_isnil(r))
 {
  hashnext(t, 0);
 }
 else
 {
  int h = present (t, luaI_Address(r));
  hashnext(t, h+1);
 }
}
