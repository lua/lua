/*
** tree.h
** TecCGraf - PUC-Rio
** $Id: tree.h,v 1.12 1996/02/12 18:32:40 roberto Exp roberto $
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
  int marked;   /* for garbage collection; never collect (nor change) if > 1 */
  char str[1];   /* \0 byte already reserved */
} TaggedString;
 

TaggedString *lua_createstring (char *str);
Long lua_strcollector (void);

#endif
