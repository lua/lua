/*
** tree.h
** TecCGraf - PUC-Rio
** $Id: tree.h,v 1.10 1995/10/17 11:53:53 roberto Exp $
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
Long lua_strcollector (void);
TreeNode *luaI_nodebysymbol (Word symbol);

#endif
