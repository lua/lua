/*
** $Id: func.h,v 1.10 1997/07/29 19:44:02 roberto Exp roberto $
*/

#ifndef func_h
#define func_h

#include "types.h"
#include "lua.h"
#include "tree.h"

typedef struct LocVar
{
  TaggedString *varname;           /* NULL signals end of scope */
  int       line;
} LocVar;


/*
** Function Headers
*/
typedef struct TFunc
{
  struct TFunc	*next;
  int		marked;
  Byte		*code;
  int		lineDefined;
  char		*fileName;
  LocVar        *locvars;
} TFunc;

TFunc *luaI_funccollector (long *cont);
void luaI_funcfree (TFunc *l);
void luaI_insertfunction (TFunc *f);

void luaI_initTFunc (TFunc *f);
void luaI_freefunc (TFunc *f);

char *luaI_getlocalname (TFunc *func, int local_number, int line);

#endif
