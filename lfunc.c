/*
** $Id: lfunc.c,v 1.13 1999/10/14 19:46:57 roberto Exp roberto $
** Auxiliary functions to manipulate prototypes and closures
** See Copyright Notice in lua.h
*/


#include <stdlib.h>

#include "lfunc.h"
#include "lmem.h"
#include "lstate.h"

#define gcsizeproto(p)	numblocks(0, sizeof(TProtoFunc))
#define gcsizeclosure(c) numblocks(c->nelems, sizeof(Closure))



Closure *luaF_newclosure (int nelems) {
  Closure *c = (Closure *)luaM_malloc(sizeof(Closure)+nelems*sizeof(TObject));
  c->next = L->rootcl;
  L->rootcl = c;
  c->marked = 0;
  c->nelems = nelems;
  L->nblocks += gcsizeclosure(c);
  return c;
}


TProtoFunc *luaF_newproto (void) {
  TProtoFunc *f = luaM_new(TProtoFunc);
  f->code = NULL;
  f->lineDefined = 0;
  f->source = NULL;
  f->consts = NULL;
  f->nconsts = 0;
  f->locvars = NULL;
  f->next = L->rootproto;
  L->rootproto = f;
  f->marked = 0;
  L->nblocks += gcsizeproto(f);
  return f;
}


void luaF_freeproto (TProtoFunc *f) {
  L->nblocks -= gcsizeproto(f);
  luaM_free(f->code);
  luaM_free(f->locvars);
  luaM_free(f->consts);
  luaM_free(f);
}


void luaF_freeclosure (Closure *c) {
  L->nblocks -= gcsizeclosure(c);
  luaM_free(c);
}


/*
** Look for n-th local variable at line "line" in function "func".
** Returns NULL if not found.
*/
const char *luaF_getlocalname (const TProtoFunc *func,
                               int local_number, int line) {
  int count = 0;
  const char *varname = NULL;
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

