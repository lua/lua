/*
** tree.h
** TecCGraf - PUC-Rio
** $Id: $
*/

#ifndef tree_h
#define tree_h

#include "opcode.h"


#define UNMARKED_STRING	0xFFFF
#define MARKED_STRING   0xFFFE
#define MAX_WORD        0xFFFD

#define indexstring(s) (*(((Word *)s)-1))


char *lua_strcreate    (char *str);
char *lua_constcreate  (char *str);
char *lua_varcreate    (char *str);
void  lua_strcollector (void);
char *lua_varnext      (char *n);
char *lua_varname      (Word index);

#endif
