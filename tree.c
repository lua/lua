/*
** tree.c
** TecCGraf - PUC-Rio
*/
 
char *rcs_tree="$Id: $";


#include <stdlib.h>
#include <string.h>

#include "lua.h"
#include "tree.h"


#define lua_strcmp(a,b)	(a[0]<b[0]?(-1):(a[0]>b[0]?(1):strcmp(a,b))) 


typedef struct TreeNode
{
 struct TreeNode *right;
 struct TreeNode *left;
 Word            index;
 char            str[1];        /* \0 byte already reserved */
} TreeNode;

static TreeNode *string_root = NULL;
static TreeNode *constant_root = NULL;
static TreeNode *variable_root = NULL;

/*
** Insert a new string/constant/variable at the tree. 
*/
static char *tree_create (TreeNode **node, char *str)
{
 if (*node == NULL)
 {
  *node = (TreeNode *) malloc (sizeof(TreeNode)+strlen(str));
  if (*node == NULL)
   lua_error ("memoria insuficiente\n");
  (*node)->left = (*node)->right = NULL;
  strcpy((*node)->str, str);
  (*node)->index = UNMARKED_STRING;
  return (*node)->str;
 }
 else
 {
  int c = lua_strcmp(str, (*node)->str);
  if (c < 0) 
   return tree_create(&(*node)->left, str);
  else if (c > 0)
   return tree_create(&(*node)->right, str);
  else
   return (*node)->str;
 }
}

char *lua_strcreate (char *str) 
{
 return tree_create(&string_root, str);
}

char *lua_constcreate (char *str) 
{
 return tree_create(&constant_root, str);
}

char *lua_varcreate (char *str) 
{
 return tree_create(&variable_root, str);
}



/*
** Free a node of the tree
*/
static TreeNode *lua_strfree (TreeNode *parent)
{
 if (parent->left == NULL && parent->right == NULL) /* no child */
 {
  free (parent);
  return NULL;
 }
 else if (parent->left == NULL)         /* only right child */
 {
  TreeNode *p = parent->right;
  free (parent);
  return p;
 }
 else if (parent->right == NULL)        /* only left child */
 {
  TreeNode *p = parent->left;
  free (parent);
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
 if (r->index == UNMARKED_STRING) 
  return lua_strfree(r);
 else
 {
  r->index = UNMARKED_STRING;
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
#if 0
 if (node == NULL) return NULL;
 if (str == NULL || lua_strcmp(str, node->str) < 0)
 {
  TreeNode *result = tree_next(node->left, str);
  return result == NULL ? node : result;
 }
 else
 {
  return tree_next(node->right, str);
 }
#else
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
#endif
}

char *lua_varnext (char *n)
{
 TreeNode *result = tree_next(variable_root, n);
 return result != NULL ? result->str : NULL;
}


/*
** Given an id, find the string with exaustive search
*/
static char *tree_name (TreeNode *node, Word index)
{
 if (node == NULL) return NULL;
 if (node->index == index) return node->str;
 else
 {
  char *result = tree_name(node->left, index);
  return result != NULL ? result : tree_name(node->right, index);
 }
}
char *lua_varname (Word index)
{
 return tree_name(variable_root, index);
}
