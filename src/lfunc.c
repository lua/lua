/*
** $Id: lfunc.c,v 1.10 1999/03/04 21:17:26 roberto Exp $
** Auxiliary functions to manipulate prototypes and closures
** See Copyright Notice in lua.h
*/


#include <stdlib.h>

#include "lfunc.h"
#include "lmem.h"
#include "lstate.h"

#define gcsizeproto(p)	5  /* approximate "weight" for a prototype */
#define gcsizeclosure(c) 1  /* approximate "weight" for a closure */



Closure *luaF_newclosure (int nelems)
{
  Closure *c = (Closure *)luaM_malloc(sizeof(Closure)+nelems*sizeof(TObject));
  luaO_insertlist(&(L->rootcl), (GCnode *)c);
  L->nblocks += gcsizeclosure(c);
  c->nelems = nelems;
  return c;
}


TProtoFunc *luaF_newproto (void)
{
  TProtoFunc *f = luaM_new(TProtoFunc);
  f->code = NULL;
  f->lineDefined = 0;
  f->source = NULL;
  f->consts = NULL;
  f->nconsts = 0;
  f->locvars = NULL;
  luaO_insertlist(&(L->rootproto), (GCnode *)f);
  L->nblocks += gcsizeproto(f);
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
    L->nblocks -= gcsizeproto(l);
    freefunc(l);
    l = next;
  }
}


void luaF_freeclosure (Closure *l)
{
  while (l) {
    Closure *next = (Closure *)l->head.next;
    L->nblocks -= gcsizeclosure(l);
    luaM_free(l);
    l = next;
  }
}


/*
** Look for n-th local variable at line "line" in function "func".
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

