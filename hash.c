/*
** hash.c
** hash manager for lua
*/

char *rcs_hash="$Id: hash.c,v 2.13 1994/11/07 15:19:51 roberto Exp roberto $";

#include <string.h>
#include <stdlib.h>

#include "opcode.h"
#include "hash.h"
#include "inout.h"
#include "table.h"
#include "lua.h"

#define streq(s1,s2)	(s1 == s2 || (*(s1) == *(s2) && strcmp(s1,s2)==0))

#define new(s)		((s *)malloc(sizeof(s)))
#define newvector(n,s)	((s *)calloc(n,sizeof(s)))

#define nhash(t)	((t)->nhash)
#define nuse(t)		((t)->nuse)
#define markarray(t)    ((t)->mark)
#define nodevector(t)	((t)->node)
#define node(t,i)	(&(t)->node[i])
#define ref(n)		(&(n)->ref)
#define val(n)		(&(n)->val)


#define REHASH_LIMIT	0.70	/* avoid more than this % full */


static Hash *listhead = NULL;



/* hash dimensions values */
static int dimensions[] = 
 {3, 5, 7, 11, 23, 47, 97, 197, 397, 797, 1597, 3203, 6421,
  12853, 25717, 51437, 0};
static int redimension (int nhash)
{
 int i;
 for (i=0; dimensions[i]!=0; i++)
 {
  if (dimensions[i] > nhash)
   return dimensions[i];
 }
 return nhash*2+1;
}

static int hashindex (Hash *t, Object *ref)		/* hash function */
{
 switch (tag(ref))
 {
  case LUA_T_NUMBER:
   return (((int)nvalue(ref))%nhash(t));
  case LUA_T_STRING:
  {
   int h;
   char *name = svalue(ref);
   for (h=0; *name!=0; name++)		/* interpret name as binary number */
    {
    h <<= 8;
    h  += (unsigned char) *name;		/* avoid sign extension */
    h  %= nhash(t);			/* make it a valid index */
   }
   return h;
  }
  case LUA_T_FUNCTION:
   return (((int)bvalue(ref))%nhash(t));
  case LUA_T_CFUNCTION:
   return (((int)fvalue(ref))%nhash(t));
  case LUA_T_ARRAY:
   return (((int)avalue(ref))%nhash(t));
  case LUA_T_USERDATA:
   return (((int)uvalue(ref))%nhash(t));
  default:
   lua_reportbug ("unexpected type to index table");
   return -1;  /* UNREACHEABLE */
 }
}

static int equalObj (Object *t1, Object *t2)
{
  if (tag(t1) != tag(t2)) return 0;
  switch (tag(t1))
  {
    case LUA_T_NUMBER: return nvalue(t1) == nvalue(t2);
    case LUA_T_STRING: return streq(svalue(t1), svalue(t2));
    default: return uvalue(t1) == uvalue(t2);
  }
}

static int present (Hash *t, Object *ref)
{ 
 int h = hashindex(t, ref);
 while (tag(ref(node(t, h))) != LUA_T_NIL)
 {
  if (equalObj(ref, ref(node(t, h))))
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
 if (v == NULL)
   lua_error ("not enough memory");
 for (i=0; i<nhash; i++)
   tag(ref(&v[i])) = LUA_T_NIL;
 return v;
}

/*
** Create a new hash. Return the hash pointer or NULL on error.
*/
static Hash *hashcreate (int nhash)
{
 Hash *t = new (Hash);
 if (t == NULL)
   lua_error ("not enough memory");
 nhash = redimension((int)((float)nhash/REHASH_LIMIT));
 
 nodevector(t) = hashnodecreate(nhash);
 if (nodevector(t) == NULL)
   lua_error ("not enough memory");
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
 free (nodevector(t));
 free(t);
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
 
/*
** Garbage collection to arrays
** Delete all unmarked arrays.
*/
void lua_hashcollector (void)
{
 Hash *curr_array = listhead, *prev = NULL;
 while (curr_array != NULL)
 {
  Hash *next = curr_array->next;
  if (markarray(curr_array) != 1)
  {
   if (prev == NULL) listhead = next;
   else              prev->next = next;
   hashdelete(curr_array);
   ++lua_recovered;
  }
  else
  {
   markarray(curr_array) = 0;
   prev = curr_array;
  }
  curr_array = next;
 }
}


/*
** Create a new array
** This function inserts the new array in the array list. It also
** executes garbage collection if the number of arrays created
** exceed a pre-defined range.
*/
Hash *lua_createarray (int nhash)
{
 Hash *array = hashcreate(nhash);
 if (lua_nentity == lua_block)
   lua_pack(); 
 lua_nentity++;
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
 nhash(t) = redimension(nhash(t));
 nodevector(t) = hashnodecreate(nhash(t));
 for (i=0; i<nold; i++)
 {
  Node *n = vold+i;
  if (tag(ref(n)) != LUA_T_NIL && tag(val(n)) != LUA_T_NIL)
   *node(t, present(t, ref(n))) = *n;  /* copy old node to new hahs */
 }
 free(vold);
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
 if (o == 0 || r == 0)
   lua_error ("too few arguments to function `next'");
 if (lua_getparam(3) != 0)
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
