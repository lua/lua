#include <string.h>

#include "luadebug.h"
#include "table.h"
#include "mem.h"
#include "func.h"
#include "opcode.h"


static TFunc *function_root = NULL;
static LocVar *currvars = NULL;
static int numcurrvars = 0;
static int maxcurrvars = 0;


/*
** Initialize TFunc struct
*/
void luaI_initTFunc (TFunc *f)
{
  f->next = NULL;
  f->marked = 0;
  f->size = 0;
  f->code = NULL;
  f->lineDefined = 0;
  f->fileName = NULL;
  f->locvars = NULL;
}

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
void luaI_freefunc (TFunc *f)
{
  luaI_free (f->code);
  luaI_free (f->locvars);
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
      luaI_freefunc (curr);
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

/*
** Stores information to know that variable has been declared in given line
*/
void luaI_registerlocalvar (TaggedString *varname, int line)
{
  if (numcurrvars >= maxcurrvars)
    maxcurrvars = growvector(&currvars, maxcurrvars, LocVar, "", MAX_WORD);
  currvars[numcurrvars].varname = varname;
  currvars[numcurrvars].line = line;
  numcurrvars++;
}

/*
** Stores information to know that variable has been out of scope in given line
*/
void luaI_unregisterlocalvar (int line)
{
  luaI_registerlocalvar(NULL, line);
}

/*
** Copies "currvars" into a new area and store it in function header.
** The values (varname = NULL, line = -1) signal the end of vector.
*/
void luaI_closelocalvars (TFunc *func)
{
  func->locvars = newvector (numcurrvars+1, LocVar);
  memcpy (func->locvars, currvars, numcurrvars*sizeof(LocVar));
  func->locvars[numcurrvars].varname = NULL;
  func->locvars[numcurrvars].line = -1;
  numcurrvars = 0;  /* prepares for next function */
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

