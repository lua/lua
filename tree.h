/*
** tree.h
** TecCGraf - PUC-Rio
** $Id: tree.h,v 1.4 1994/11/17 13:58:57 roberto Exp roberto $
*/

#ifndef tree_h
#define tree_h

#include "opcode.h"


#define UNMARKED_STRING	0xFFFF
#define MARKED_STRING   0xFFFE
#define MAX_WORD        0xFFFD

 
typedef struct TreeNode
{
 struct TreeNode *right;
 struct TreeNode *left;
 Word varindex;  /* if this is a symbol */
 Word constindex;  /* if this is a constant; also used for garbage collection */
 char            str[1];        /* \0 byte already reserved */
} TreeNode;


#define indexstring(s) (*(((Word *)s)-1))


char *lua_createstring (char *str);
TreeNode *lua_constcreate  (char *str);
int  lua_strcollector (void);
TreeNode *lua_varnext      (char *n);

#endif
