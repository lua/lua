/*
** hash.c
** hash manager for lua
*/

char *rcs_hash="$Id: hash.c,v 2.4 1994/08/09 11:24:45 celes Exp celes $";

#include <string.h>
#include <stdlib.h>

#include "mm.h"

#include "opcode.h"
#include "hash.h"
#include "inout.h"
#include "table.h"
#include "lua.h"

#define streq(s1,s2)	(*(s1) == *(s2) && strcmp(s1,s2)==0)

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


typedef struct ArrayList
{
 Hash *array;
 struct ArrayList *next;
} ArrayList;

static ArrayList *listhead = NULL;



/* hash dimensions values */
static int dimensions[] = 
 {7, 11, 23, 47, 97, 197, 397, 797, 1597, 3203, 6421, 12853, 25717, 51437, 0};
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

static int index (Hash *t, Object *ref)		/* hash function */
{
 switch (tag(ref))
 {
  case T_NUMBER:
   return (((int)nvalue(ref))%nhash(t));
  case T_STRING:
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
  case T_FUNCTION:
   return (((int)bvalue(ref))%nhash(t));
  case T_CFUNCTION:
   return (((int)fvalue(ref))%nhash(t));
  case T_ARRAY:
   return (((int)avalue(ref))%nhash(t));
  case T_USERDATA:
   return (((int)uvalue(ref))%nhash(t));
  default:
   lua_reportbug ("unexpected type to index table");
   return -1;
 }
}

static int present (Hash *t, Object *ref)
{ 
 int h = index(t, ref);
 if (h < 0) return h;
 while (tag(ref(node(t, h))) != T_NIL)
 {
  if (tag(ref) == T_NUMBER && tag(ref(node(t, h))) == T_NUMBER && 
      nvalue(ref) == nvalue(ref(node(t, h)))
     ) return h;
  if (tag(ref) == T_STRING && tag(ref(node(t, h))) == T_STRING && 
      streq(svalue(ref),svalue(ref(node(t, h))))
     ) return h;
  if (tag(ref) == tag(ref(node(t, h))) &&
      uvalue(ref) == uvalue(ref(node(t, h)))	/* all others are pointers */
     ) return h;
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
 {
  lua_error ("not enough memory");
  return NULL;
 }
 for (i=0; i<nhash; i++)
  tag(ref(&v[i])) = T_NIL;
 return v;
}

/*
** Create a new hash. Return the hash pointer or NULL on error.
*/
static Hash *hashcreate (int nhash)
{
 Hash *t = new (Hash);
 if (t == NULL)
 {
  lua_error ("not enough memory");
  return NULL;
 }
 nhash = redimension(nhash);
 
 nodevector(t) = hashnodecreate(nhash);
 if (nodevector(t) == NULL)
  return NULL;
 
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
   if (tag(ref(n)) != T_NIL)
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
 ArrayList *curr = listhead, *prev = NULL;
 while (curr != NULL)
 {
  ArrayList *next = curr->next;
  if (markarray(curr->array) != 1)
  {
   if (prev == NULL) listhead = next;
   else              prev->next = next;
   hashdelete(curr->array);
   free(curr);
  }
  else
  {
   markarray(curr->array) = 0;
   prev = curr;
  }
  curr = next;
 }
}


/*
** Create a new array
** This function insert the new array at array list. It also
** execute garbage collection if the number of array created
** exceed a pre-defined range.
*/
Hash *lua_createarray (int nhash)
{
 ArrayList *new = new(ArrayList);
 if (new == NULL)
 {
  lua_error ("not enough memory");
  return NULL;
 }
 new->array = hashcreate(nhash);
 if (new->array == NULL)
 {
  lua_error ("not enough memory");
  return NULL;
 }

 if (lua_nentity == lua_block)
  lua_pack(); 

 lua_nentity++;
 new->next = listhead;
 listhead = new;
 return new->array;
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
  if (tag(ref(n)) != T_NIL && tag(val(n)) != T_NIL)
   *node(t, present(t, ref(n))) = *n;  /* copy old node to new hahs */
 }
 free(vold);
}

/*
** If the hash node is present, return its pointer, otherwise search
** the node at parent table, recursively, if there is parent.
** If no parent and the node is not present, return a static nil object.
*/
Object *lua_hashget (Hash *t, Object *ref)
{
 static Object nil_obj = {T_NIL, {NULL}};
 Object parent;
 int count = 1000; 
 tag(&parent) = T_STRING;
 svalue(&parent) = "parent";
 do
 {
  int h = present(t, ref);
  if (h < 0) return NULL; 
  if (tag(ref(node(t, h))) != T_NIL) return val(node(t, h));
  
  h = present(t, &parent); /* assert(p >= 0); */
  t = tag(ref(node(t, h))) != T_NIL && tag(val(node(t, h))) == T_ARRAY ?
      avalue(val(node(t, h))) : NULL;
 } while (t != NULL && --count);
 if (count == 0) 
   lua_reportbug ("hierarchy too deep (maybe there is an inheritance loop)");
 return &nil_obj;
}

/*
** If the hash node is present, return its pointer, otherwise create a new
** node for the given reference and also return its pointer.
** On error, return NULL.
*/
Object *lua_hashdefine (Hash *t, Object *ref)
{
 int   h;
 Node *n;
 h = present(t, ref);
 if (h < 0) return NULL; 
 n = node(t, h);
 if (tag(ref(n)) == T_NIL)
 {
  nuse(t)++;
  if (nuse(t) > nhash(t)*REHASH_LIMIT)
  {
   rehash(t);
   h = present(t, ref);
   n = node(t, h);
  }
  *ref(n) = *ref;
  tag(val(n)) = T_NIL;
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
 while (tag(ref(node(t,i))) == T_NIL || tag(val(node(t,i))) == T_NIL)
 {
  if (++i >= nhash(t))
  {
   lua_pushnil(); lua_pushnil();
   return;
  }
 }
 lua_pushobject(ref(node(t,i)));
 lua_pushobject(val(node(t,i)));
}

void lua_next (void)
{
 Hash   *t;
 Object *o = lua_getparam (1);
 Object *r = lua_getparam (2);
 if (o == NULL || r == NULL)
 { lua_error ("too few arguments to function `next'"); return; }
 if (lua_getparam (3) != NULL)
 { lua_error ("too many arguments to function `next'"); return; }
 if (tag(o) != T_ARRAY)
 { lua_error ("first argument of function `next' is not a table"); return; }

 t = avalue(o);
 if (tag(r) == T_NIL)
 {
  hashnext(t, 0);
 }
 else
 {
  int h = present (t, r);
  if (h >= 0)
   hashnext(t, h+1);
  else
   lua_error ("error in function 'next': reference not found");
 }
}
