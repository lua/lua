/*
** tree.h
** TecCGraf - PUC-Rio
** $Id: tree.h,v 1.1 1994/07/19 21:24:17 celes Exp roberto $
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


char *lua_strcreate    (char *str);
TreeNode *lua_constcreate  (char *str);
void  lua_strcollector (void);
char *lua_varnext      (char *n);

#endif
