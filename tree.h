/*
** tree.h
** TecCGraf - PUC-Rio
** $Id: tree.h,v 1.16 1997/03/19 19:41:10 roberto Exp roberto $
*/

#ifndef tree_h
#define tree_h

#include "types.h"

#define NOT_USED  0xFFFE


typedef struct TaggedString
{
  int tag;  /* if != LUA_T_STRING, this is a userdata */
  struct TaggedString *next;
  long size;
  Word varindex;  /* != NOT_USED  if this is a symbol */
  Word constindex;  /* != NOT_USED  if this is a constant */
  unsigned long hash;  /* 0 if not initialized */
  int marked;   /* for garbage collection; never collect (nor change) if > 1 */
  char str[1];   /* \0 byte already reserved; MAY BE NOT 0 TERMINATED!! */
} TaggedString;
 

TaggedString *lua_createstring (char *str);
TaggedString *luaI_createuserdata (char *buff, long size, int tag);
TaggedString *luaI_strcollector (long *cont);
void luaI_strfree (TaggedString *l);
void luaI_strcallIM (TaggedString *l);

#endif
