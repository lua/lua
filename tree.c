/*
** tree.c
** TecCGraf - PUC-Rio
*/
 
char *rcs_tree="$Id: tree.c,v 1.5 1994/11/16 16:03:48 roberto Exp roberto $";


#include <string.h>

#include "mem.h"
#include "lua.h"
#include "tree.h"
#include "table.h"


#define lua_strcmp(a,b)	(a[0]<b[0]?(-1):(a[0]>b[0]?(1):strcmp(a,b))) 


static TreeNode *string_root = NULL;
static TreeNode *constant_root = NULL;

/*
** Insert a new string/constant/variable at the tree. 
*/
static TreeNode *tree_create (TreeNode **node, char *str, int *created)
{
 if (*node == NULL)
 {
  *node = (TreeNode *) luaI_malloc(sizeof(TreeNode)+strlen(str));
  (*node)->left = (*node)->right = NULL;
  strcpy((*node)->str, str);
  (*node)->varindex = (*node)->constindex = UNMARKED_STRING;
  *created = 1;
  return *node;
 }
 else
 {
  int c = lua_strcmp(str, (*node)->str);
  if (c < 0) 
   return tree_create(&(*node)->left, str, created);
  else if (c > 0)
   return tree_create(&(*node)->right, str, created);
  else
   return *node;
 }
}

char *lua_strcreate (char *str) 
{
 int created=0;
 TreeNode *t = tree_create(&string_root, str, &created);
 if (created)
 {
  if (lua_nentity == lua_block) lua_pack ();
  lua_nentity++;
 }
 return t->str;
}

TreeNode *lua_constcreate (char *str) 
{
 int created;
 return tree_create(&constant_root, str, &created);
}


/*
** Free a node of the tree
*/
static TreeNode *lua_strfree (TreeNode *parent)
{
 if (parent->left == NULL && parent->right == NULL) /* no child */
 {
  luaI_free(parent);
  return NULL;
 }
 else if (parent->left == NULL)         /* only right child */
 {
  TreeNode *p = parent->right;
  luaI_free(parent);
  return p;
 }
 else if (parent->right == NULL)        /* only left child */
 {
  TreeNode *p = parent->left;
  luaI_free(parent);
  return p;
 }
 else                                   /* two children */
 {
  TreeNode *p = parent, *r = parent->right;
  while (r->left != NULL)
  {
   p = r;
   r = r->left;
  }
  if (p == parent)
  {
   r->left  = parent->left;
   parent->left = NULL;
   parent->right = r->right;
   r->right = lua_strfree(parent);
  }
  else
  {
   TreeNode *t = r->right;
   r->left  = parent->left;
   r->right = parent->right;
   parent->left = NULL;
   parent->right = t;
   p->left  = lua_strfree(parent);
  }
  return r;
 }
}

/*
** Traverse tree for garbage collection
*/
static TreeNode *lua_travcollector (TreeNode *r)
{
 if (r == NULL) return NULL;
 r->right = lua_travcollector(r->right);
 r->left  = lua_travcollector(r->left);
 if (r->constindex == UNMARKED_STRING) 
 {
  ++lua_recovered;
  return lua_strfree(r);
 }
 else
 {
  r->constindex = UNMARKED_STRING;
  return r;
 }
}


/*
** Garbage collection function.
** This function traverse the tree freening unindexed strings
*/
void lua_strcollector (void)
{
 string_root = lua_travcollector(string_root);
}

/*
** Return next variable.
*/
static TreeNode *tree_next (TreeNode *node, char *str)
{
 if (node == NULL) return NULL;
 else if (str == NULL) return node;
 else
 {
  int c = lua_strcmp(str, node->str);
  if (c == 0)
   return node->left != NULL ? node->left : node->right;
  else if (c < 0)
  {
   TreeNode *result = tree_next(node->left, str);
   return result != NULL ? result : node->right;
  }
  else
   return tree_next(node->right, str);
 }
}

TreeNode *lua_varnext (char *n)
{
  TreeNode *result;
  char *name = n;
  while (1)
  { /* repeat until a valid (non nil) variable */
    result = tree_next(constant_root, name);
    if (result == NULL) return NULL;
    if (result->varindex != UNMARKED_STRING &&
        s_tag(result->varindex) != LUA_T_NIL)
      return result;
    name = result->str;
  }
}

