/*
** tree.h
** TecCGraf - PUC-Rio
** $Id: tree.h,v 1.11 1996/01/26 18:03:19 roberto Exp roberto $
*/

#ifndef tree_h
#define tree_h

#include "types.h"

#define NOT_USED  0xFFFE


typedef struct TaggedString
{
  unsigned short varindex;  /* != NOT_USED  if this is a symbol */
  unsigned short constindex;  /* != NOT_USED  if this is a constant */
  unsigned long hash;  /* 0 if not initialized */
  char marked;   /* for garbage collection; 2 means "never collect" */
  char str[1];   /* \0 byte already reserved */
} TaggedString;
 

TaggedString *lua_createstring (char *str);
TaggedString *luaI_createfixedstring (char *str);
Long lua_strcollector (void);

#endif
