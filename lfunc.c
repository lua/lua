/*
** $Id: lfunc.c,v 1.25 2000/06/26 19:28:31 roberto Exp roberto $
** Auxiliary functions to manipulate prototypes and closures
** See Copyright Notice in lua.h
*/


#include <stdlib.h>

#define LUA_REENTRANT

#include "lua.h"

#include "lfunc.h"
#include "lmem.h"
#include "lstate.h"

#define gcsizeproto(L, p)	numblocks(L, 0, sizeof(Proto))
#define gcsizeclosure(L, c)	numblocks(L, c->nupvalues, sizeof(Closure))



Closure *luaF_newclosure (lua_State *L, int nelems) {
  Closure *c = (Closure *)luaM_malloc(L, sizeof(Closure) +
                                         (lint32)sizeof(TObject)*(nelems-1));
  c->next = L->rootcl;
  L->rootcl = c;
  c->mark = c;
  c->nupvalues = nelems;
  L->nblocks += gcsizeclosure(L, c);
  return c;
}


Proto *luaF_newproto (lua_State *L) {
  Proto *f = luaM_new(L, Proto);
  f->code = NULL;
  f->lines = NULL;
  f->lineDefined = 0;
  f->source = NULL;
  f->kstr = NULL;
  f->nkstr = 0;
  f->knum = NULL;
  f->nknum = 0;
  f->kproto = NULL;
  f->nkproto = 0;
  f->locvars = NULL;
  f->next = L->rootproto;
  L->rootproto = f;
  f->marked = 0;
  L->nblocks += gcsizeproto(L, f);
  return f;
}


void luaF_freeproto (lua_State *L, Proto *f) {
  L->nblocks -= gcsizeproto(L, f);
  luaM_free(L, f->code);
  luaM_free(L, f->locvars);
  luaM_free(L, f->kstr);
  luaM_free(L, f->knum);
  luaM_free(L, f->kproto);
  luaM_free(L, f->lines);
  luaM_free(L, f);
}


void luaF_freeclosure (lua_State *L, Closure *c) {
  L->nblocks -= gcsizeclosure(L, c);
  luaM_free(L, c);
}


/*
** Look for n-th local variable at line `line' in function `func'.
** Returns NULL if not found.
*/
const char *luaF_getlocalname (const Proto *func, int local_number, int pc) {
  int count = 0;
  const char *varname = NULL;
  LocVar *lv = func->locvars;
  if (lv == NULL)
    return NULL;
  for (; lv->pc != -1 && lv->pc <= pc; lv++) {
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

