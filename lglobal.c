/*
** $Id: $
** Global variables
** See Copyright Notice in lua.h
*/

#include <stdlib.h>

#include "lbuiltin.h"
#include "lglobal.h"
#include "lmem.h"
#include "lobject.h"
#include "lstring.h"


Symbol *luaG_global = NULL;
int luaG_nglobal = 0;
static int maxglobal = 0;



Word luaG_findsymbol (TaggedString *t)
{
  if (maxglobal == 0) {  /* first time? */
    maxglobal = 50;
    luaG_global = luaM_newvector(maxglobal, Symbol);
    luaB_predefine();
  }
  if (t->u.s.varindex == NOT_USED) {
    if (!t->marked) t->marked = 2;  /* avoid GC of global variable names */
    if (luaG_nglobal >= maxglobal)
      maxglobal = luaM_growvector(&luaG_global, maxglobal, Symbol,
                             symbolEM, MAX_WORD);
    t->u.s.varindex = luaG_nglobal;
    luaG_global[luaG_nglobal].varname = t;
    s_ttype(luaG_nglobal) = LUA_T_NIL;
    luaG_nglobal++;
  }
  return t->u.s.varindex;
}


Word luaG_findsymbolbyname (char *name)
{
  return luaG_findsymbol(luaS_new(name));
}


int luaG_globaldefined (char *name)
{
  return s_ttype(luaG_findsymbolbyname(name)) != LUA_T_NIL;
}


int luaG_nextvar (Word next)
{
  while (next < luaG_nglobal && s_ttype(next) == LUA_T_NIL)
    next++;
  return (next < luaG_nglobal ? next : -1);
}


char *luaG_travsymbol (int (*fn)(TObject *))
{
  int i;
  for (i=0; i<luaG_nglobal; i++)
    if (fn(&s_object(i)))
      return luaG_global[i].varname->str;
  return NULL;
}

