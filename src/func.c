#include <stdio.h>

#include "luadebug.h"
#include "table.h"
#include "mem.h"
#include "func.h"
#include "opcode.h"

static TFunc *function_root = NULL;


/*
** Insert function in list for GC
*/
void luaI_insertfunction (TFunc *f)
{
  lua_pack();
  f->next = function_root;
  function_root = f;
  f->marked = 0;
}


/*
** Free function
*/
static void freefunc (TFunc *f)
{
  luaI_free (f->code);
  luaI_free (f);
}

/*
** Garbage collection function.
** This function traverse the function list freeing unindexed functions
*/
Long luaI_funccollector (void)
{
  TFunc *curr = function_root;
  TFunc *prev = NULL;
  Long counter = 0;
  while (curr)
  {
    TFunc *next = curr->next;
    if (!curr->marked)
    {
      if (prev == NULL)
        function_root = next;
      else
        prev->next = next;
      freefunc (curr);
      ++counter;
    }
    else
    {
      curr->marked = 0;
      prev = curr;
    } 
    curr = next;
  }
  return counter;
}


void lua_funcinfo (lua_Object func, char **filename, int *linedefined)
{
  Object *f = luaI_Address(func);
  if (f->tag == LUA_T_MARK || f->tag == LUA_T_FUNCTION)
  {
    *filename = f->value.tf->fileName;
    *linedefined = f->value.tf->lineDefined;
  }
  else if (f->tag == LUA_T_CMARK || f->tag == LUA_T_CFUNCTION)
  {
    *filename = "(C)";
    *linedefined = -1;
  }
}

