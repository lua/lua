/*
** tree.h
** TecCGraf - PUC-Rio
** $Id: tree.h,v 1.5 1994/11/18 19:27:38 roberto Exp roberto $
*/

#ifndef tree_h
#define tree_h


#define NOT_USED  0xFFFE


typedef struct TaggedString
{
  char marked;   /* for garbage collection */
  char str[1];   /* \0 byte already reserved */
} TaggedString;
 
typedef struct TreeNode
{
 struct TreeNode *right;
 struct TreeNode *left;
 unsigned short varindex;  /* != NOT_USED  if this is a symbol */
 unsigned short constindex;  /* != NOT_USED  if this is a constant */
 TaggedString ts;
} TreeNode;


TaggedString *lua_createstring (char *str);
TreeNode *lua_constcreate  (char *str);
int  lua_strcollector (void);
TreeNode *lua_varnext      (char *n);

#endif
