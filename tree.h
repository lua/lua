/*
** tree.h
** TecCGraf - PUC-Rio
** $Id: tree.h,v 1.7 1994/11/25 19:27:03 roberto Exp roberto $
*/

#ifndef tree_h
#define tree_h

#include "types.h"

#define NOT_USED  0xFFFE


typedef struct TaggedString
{
  unsigned long hash;  /* 0 if not initialized */
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
Word lua_strcollector (void);
TreeNode *lua_varnext      (char *n);

#endif
