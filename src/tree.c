/*
** tree.c
** TecCGraf - PUC-Rio
*/
 
char *rcs_tree="$Id: tree.c,v 1.14 1995/10/17 11:53:53 roberto Exp $";


#include <string.h>

#include "mem.h"
#include "lua.h"
#include "tree.h"
#include "table.h"


#define lua_strcmp(a,b)	(a[0]<b[0]?(-1):(a[0]>b[0]?(1):strcmp(a,b))) 


typedef struct StringNode {
  struct StringNode *next;
  TaggedString ts;
} StringNode;

static StringNode *string_root = NULL;

static TreeNode *constant_root = NULL;

/*
** Insert a new constant/variable at the tree. 
*/
static TreeNode *tree_create (TreeNode **node, char *str)
{
 if (*node == NULL)
 {
  *node = (TreeNode *) luaI_malloc(sizeof(TreeNode)+strlen(str));
  (*node)->left = (*node)->right = NULL;
  strcpy((*node)->ts.str, str);
  (*node)->ts.marked = 0;
  (*node)->ts.hash = 0;
  (*node)->varindex = (*node)->constindex = NOT_USED;
  return *node;
 }
 else
 {
  int c = lua_strcmp(str, (*node)->ts.str);
  if (c < 0) 
   return tree_create(&(*node)->left, str);
  else if (c > 0)
   return tree_create(&(*node)->right, str);
  else
   return *node;
 }
}

TaggedString *lua_createstring (char *str) 
{
  StringNode *newString;
  if (str == NULL) return NULL;
  lua_pack();
  newString = (StringNode *)luaI_malloc(sizeof(StringNode)+strlen(str));
  newString->ts.marked = 0;
  newString->ts.hash = 0;
  strcpy(newString->ts.str, str);
  newString->next = string_root;
  string_root = newString;
  return &(newString->ts);
}


TreeNode *lua_constcreate (char *str) 
{
 return tree_create(&constant_root, str);
}


/*
** Garbage collection function.
** This function traverse the string list freeing unindexed strings
*/
Long lua_strcollector (void)
{
  StringNode *curr = string_root, *prev = NULL;
  Long counter = 0;
  while (curr)
  {
    StringNode *next = curr->next;
    if (!curr->ts.marked)
    {
      if (prev == NULL) string_root = next;
      else prev->next = next;
      luaI_free(curr);
      ++counter;
    }
    else
    {
      curr->ts.marked = 0;
      prev = curr;
    }
    curr = next;
  }
  return counter;
}


/*
** Traverse the constant tree looking for a specific symbol number
*/
static TreeNode *nodebysymbol (TreeNode *root, Word symbol)
{
  TreeNode *t;
  if (root == NULL) return NULL;
  if (root->varindex == symbol) return root;
  t = nodebysymbol(root->left, symbol);
  if (t) return t;
  return nodebysymbol(root->right, symbol);
}

TreeNode *luaI_nodebysymbol (Word symbol)
{
  return nodebysymbol(constant_root, symbol); 
}

