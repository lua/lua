/*
** $Id: func.h,v 1.8 1996/03/14 15:54:20 roberto Exp $
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
  int		size;
  Byte		*code;
  int		lineDefined;
  char		*fileName;
  LocVar        *locvars;
} TFunc;

Long luaI_funccollector (void);
void luaI_insertfunction (TFunc *f);

void luaI_initTFunc (TFunc *f);
void luaI_freefunc (TFunc *f);

void luaI_registerlocalvar (TaggedString *varname, int line);
void luaI_unregisterlocalvar (int line);
void luaI_closelocalvars (TFunc *func);
char *luaI_getlocalname (TFunc *func, int local_number, int line);

#endif
