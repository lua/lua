#ifndef func_h
#define func_h

#include "types.h"
#include "lua.h"

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
  char		*fileName;
} TFunc;

Long luaI_funccollector (void);
void luaI_insertfunction (TFunc *f);


#endif
