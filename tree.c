/*
** tree.c
** TecCGraf - PUC-Rio
*/
 
char *rcs_tree="$Id: tree.c,v 1.6 1994/11/16 17:38:08 roberto Exp roberto $";


#include <string.h>

#include "mem.h"
#include "lua.h"
#include "tree.h"
#include "table.h"


#define lua_strcmp(a,b)	(a[0]<b[0]?(-1):(a[0]>b[0]?(1):strcmp(a,b))) 


typedef struct StringNode {
  struct StringNode *next;
  Word mark;
  char str[1];
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
  strcpy((*node)->str, str);
  (*node)->varindex = (*node)->constindex = UNMARKED_STRING;
  return *node;
 }
 else
 {
  int c = lua_strcmp(str, (*node)->str);
  if (c < 0) 
   return tree_create(&(*node)->left, str);
  else if (c > 0)
   return tree_create(&(*node)->right, str);
  else
   return *node;
 }
}

char *lua_strcreate (char *str) 
{
  StringNode *newString = (StringNode *)luaI_malloc(sizeof(StringNode)+
                                                strlen(str));
  newString->mark = UNMARKED_STRING;
  strcpy(newString->str, str);
  newString->next = string_root;
  string_root = newString;
  if (lua_nentity == lua_block) lua_pack ();
  lua_nentity++;
  return newString->str;
}


TreeNode *lua_constcreate (char *str) 
{
 return tree_create(&constant_root, str);
}


/*
** Garbage collection function.
** This function traverse the string list freeing unindexed strings
*/
void lua_strcollector (void)
{
  StringNode *curr = string_root, *prev = NULL;
  while (curr)
  {
    StringNode *next = curr->next;
    if (curr->mark == UNMARKED_STRING)
    {
      if (prev == NULL) string_root = next;
      else prev->next = next;
      luaI_free(curr);
      ++lua_recovered;
    }
    else
    {
      curr->mark = UNMARKED_STRING;
      prev = curr;
    }
    curr = next;
  }
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

