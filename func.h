#ifndef func_h
#define func_h

#include "types.h"

/*
** Header para funcoes.
*/
typedef struct TFunc
{
  struct TFunc	*next;
  char		marked;
  int		size;
  Byte		*code;
  int		lineDefined;
  char		*name1;  /* function or method name (or null if main) */
  char		*name2;  /* object name (or null if not method) */
  char		*fileName;
} TFunc;

Long luaI_funccollector (void);
void luaI_insertfunction (TFunc *f);

#endif
