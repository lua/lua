#include <string.h>

#include "luadebug.h"
#include "table.h"
#include "luamem.h"
#include "func.h"
#include "opcode.h"
#include "inout.h"


static TFunc *function_root = NULL;


static void luaI_insertfunction (TFunc *f)
{
  lua_pack();
  f->next = function_root;
  function_root = f;
  f->marked = 0;
}

/*
** Initialize TFunc struct
*/
void luaI_initTFunc (TFunc *f)
{
  f->next = NULL;
  f->marked = 0;
  f->code = NULL;
  f->lineDefined = 0;
  f->fileName = lua_parsedfile;
  f->consts = NULL;
  f->nconsts = 0;
  f->locvars = NULL;
  luaI_insertfunction(f);
}



/*
** Free function
*/
static void luaI_freefunc (TFunc *f)
{
  luaI_free(f->code);
  luaI_free(f->locvars);
  luaI_free(f->consts);
  luaI_free(f);
}


void luaI_funcfree (TFunc *l)
{
  while (l) {
    TFunc *next = l->next;
    luaI_freefunc(l);
    l = next;
  }
}


void luaI_funcmark (TFunc *f)
{
  f->marked = 1;
  if (!f->fileName->marked)
    f->fileName->marked = 1;
  if (f->consts) {
    int i;
    for (i=0; i<f->nconsts; i++)
      lua_markobject(&f->consts[i]);
  }
}


/*
** Garbage collection function.
*/
TFunc *luaI_funccollector (long *acum)
{
  TFunc *curr = function_root;
  TFunc *prev = NULL;
  TFunc *frees = NULL;
  long counter = 0;
  while (curr) {
    TFunc *next = curr->next;
    if (!curr->marked) {
      if (prev == NULL)
        function_root = next;
      else
        prev->next = next;
      curr->next = frees;
      frees = curr;
      ++counter;
    }
    else {
      curr->marked = 0;
      prev = curr;
    } 
    curr = next;
  }
  *acum += counter;
  return frees;
}


void lua_funcinfo (lua_Object func, char **filename, int *linedefined)
{
  TObject *f = luaI_Address(func);
  if (f->ttype == LUA_T_MARK || f->ttype == LUA_T_FUNCTION)
  {
    *filename = f->value.tf->fileName->str;
    *linedefined = f->value.tf->lineDefined;
  }
  else if (f->ttype == LUA_T_CMARK || f->ttype == LUA_T_CFUNCTION)
  {
    *filename = "(C)";
    *linedefined = -1;
  }
}


/*
** Look for n-esim local variable at line "line" in function "func".
** Returns NULL if not found.
*/
char *luaI_getlocalname (TFunc *func, int local_number, int line)
{
  int count = 0;
  char *varname = NULL;
  LocVar *lv = func->locvars;
  if (lv == NULL)
    return NULL;
  for (; lv->line != -1 && lv->line < line; lv++)
  {
    if (lv->varname)               /* register */
    {
      if (++count == local_number)
        varname = lv->varname->str;
    }
    else                           /* unregister */
      if (--count < local_number)
        varname = NULL;
  }
  return varname;
}

