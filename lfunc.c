/*
** $Id: lfunc.c,v 1.1 1997/09/16 19:25:59 roberto Exp roberto $
** Lua Funcion auxiliar
** See Copyright Notice in lua.h
*/


#include <stdlib.h>

#include "lfunc.h"
#include "lmem.h"


GCnode luaF_root = {NULL, 0};
GCnode luaF_rootcl = {NULL, 0};



Closure *luaF_newclosure (int nelems)
{
  Closure *c = (Closure *)luaM_malloc(sizeof(Closure)+nelems*sizeof(TObject));
  luaO_insertlist(&luaF_rootcl, (GCnode *)c);
  return c;
}


TProtoFunc *luaF_newproto (void)
{
  TProtoFunc *f = luaM_new(TProtoFunc);
  f->code = NULL;
  f->lineDefined = 0;
  f->fileName = NULL;
  f->consts = NULL;
  f->nconsts = 0;
  f->nupvalues = 0;
  f->locvars = NULL;
  luaO_insertlist(&luaF_root, (GCnode *)f);
  return f;
}



static void freefunc (TProtoFunc *f)
{
  luaM_free(f->code);
  luaM_free(f->locvars);
  luaM_free(f->consts);
  luaM_free(f);
}


void luaF_freeproto (TProtoFunc *l)
{
  while (l) {
    TProtoFunc *next = (TProtoFunc *)l->head.next;
    freefunc(l);
    l = next;
  }
}


void luaF_freeclosure (Closure *l)
{
  while (l) {
    Closure *next = (Closure *)l->head.next;
    luaM_free(l);
    l = next;
  }
}


/*
** Look for n-esim local variable at line "line" in function "func".
** Returns NULL if not found.
*/
char *luaF_getlocalname (TProtoFunc *func, int local_number, int line)
{
  int count = 0;
  char *varname = NULL;
  LocVar *lv = func->locvars;
  if (lv == NULL)
    return NULL;
  for (; lv->line != -1 && lv->line < line; lv++) {
    if (lv->varname) {  /* register */
      if (++count == local_number)
        varname = lv->varname->str;
    }
    else  /* unregister */
      if (--count < local_number)
        varname = NULL;
  }
  return varname;
}

