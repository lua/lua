/*
** hash.c
** hash manager for lua
*/

char *rcs_hash="$Id: hash.c,v 2.24 1995/02/06 19:34:03 roberto Exp $";

#include <string.h>

#include "mem.h"
#include "opcode.h"
#include "hash.h"
#include "inout.h"
#include "table.h"
#include "lua.h"

#define streq(s1,s2)	(s1 == s2 || (*(s1) == *(s2) && strcmp(s1,s2)==0))

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
static Word dimensions[] = 
 {3, 5, 7, 11, 23, 47, 97, 197, 397, 797, 1597, 3203, 6421,
  12853, 25717, 51437, 65521, 0};  /* 65521 == last prime < MAX_WORD */

static Word redimension (Word nhash)
{
 Word i;
 for (i=0; dimensions[i]!=0; i++)
 {
  if (dimensions[i] > nhash)
   return dimensions[i];
 }
 lua_error("table overflow");
 return 0;  /* to avoid warnings */
}

static Word hashindex (Hash *t, Object *ref)		/* hash function */
{
 switch (tag(ref))
 {
  case LUA_T_NIL:
   lua_reportbug ("unexpected type to index table");
   return -1;  /* UNREACHEABLE */
  case LUA_T_NUMBER:
   return (((Word)nvalue(ref))%nhash(t));
  case LUA_T_STRING:
  {
   unsigned long h = tsvalue(ref)->hash;
   if (h == 0)
   {
     char *name = svalue(ref);
     while (*name)
       h = ((h<<5)-h)^(unsigned char)*(name++);
     tsvalue(ref)->hash = h;
   }
   return (Word)h%nhash(t);  /* make it a valid index */
  }
  case LUA_T_FUNCTION:
   return (((IntPoint)bvalue(ref))%nhash(t));
  case LUA_T_CFUNCTION:
   return (((IntPoint)fvalue(ref))%nhash(t));
  case LUA_T_ARRAY:
   return (((IntPoint)avalue(ref))%nhash(t));
  default:  /* user data */
   return (((IntPoint)uvalue(ref))%nhash(t));
 }
}

Bool lua_equalObj (Object *t1, Object *t2)
{
  if (tag(t1) != tag(t2)) return 0;
  switch (tag(t1))
  {
    case LUA_T_NIL: return 1;
    case LUA_T_NUMBER: return nvalue(t1) == nvalue(t2);
    case LUA_T_STRING: return streq(svalue(t1), svalue(t2));
    case LUA_T_ARRAY: return avalue(t1) == avalue(t2);
    case LUA_T_FUNCTION: return bvalue(t1) == bvalue(t2);
    case LUA_T_CFUNCTION: return fvalue(t1) == fvalue(t2);
    default: return uvalue(t1) == uvalue(t2);
  }
}

static Word present (Hash *t, Object *ref)
{ 
 Word h = hashindex(t, ref);
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
static Node *hashnodecreate (Word nhash)
{
 Word i;
 Node *v = newvector (nhash, Node);
 for (i=0; i<nhash; i++)
   tag(ref(&v[i])) = LUA_T_NIL;
 return v;
}

/*
** Create a new hash. Return the hash pointer or NULL on error.
*/
static Hash *hashcreate (Word nhash)
{
 Hash *t = new(Hash);
 nhash = redimension((Word)((float)nhash/REHASH_LIMIT));
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
  Word i;
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
Hash *lua_createarray (Word nhash)
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
 Word i;
 Word   nold = nhash(t);
 Node *vold = nodevector(t);
 nhash(t) = redimension(nhash(t));
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
 Word h = present(t, ref);
 if (tag(ref(node(t, h))) != LUA_T_NIL) return val(node(t, h));
 else return NULL;
}


/*
** If the hash node is present, return its pointer, otherwise create a new
** node for the given reference and also return its pointer.
*/
Object *lua_hashdefine (Hash *t, Object *ref)
{
 Word   h;
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
static void hashnext (Hash *t, Word i)
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
  Word h = present (t, luaI_Address(r));
  hashnext(t, h+1);
 }
}
