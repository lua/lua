/*
** $Id: lfunc.c,v 1.45 2001/06/28 14:57:17 roberto Exp $
** Auxiliary functions to manipulate prototypes and closures
** See Copyright Notice in lua.h
*/


#include <stdlib.h>

#define LUA_PRIVATE
#include "lua.h"

#include "lfunc.h"
#include "lmem.h"
#include "lstate.h"


#define sizeclosure(n)	(cast(int, sizeof(Closure)) + \
                         cast(int, sizeof(TObject)*((n)-1)))


Closure *luaF_newclosure (lua_State *L, int nelems) {
  Closure *c = cast(Closure *, luaM_malloc(L, sizeclosure(nelems)));
  c->next = G(L)->rootcl;
  G(L)->rootcl = c;
  c->mark = c;
  c->nupvalues = nelems;
  return c;
}


Proto *luaF_newproto (lua_State *L) {
  Proto *f = luaM_new(L, Proto);
  f->k = NULL;
  f->sizek = 0;
  f->p = NULL;
  f->sizep = 0;
  f->code = NULL;
  f->sizecode = 0;
  f->nupvalues = 0;
  f->numparams = 0;
  f->is_vararg = 0;
  f->maxstacksize = 0;
  f->marked = 0;
  f->lineinfo = NULL;
  f->sizelocvars = 0;
  f->sizelineinfo = 0;
  f->locvars = NULL;
  f->lineDefined = 0;
  f->source = NULL;
  f->next = G(L)->rootproto;  /* chain in list of protos */
  G(L)->rootproto = f;
  return f;
}


void luaF_freeproto (lua_State *L, Proto *f) {
  luaM_freearray(L, f->code, f->sizecode, Instruction);
  luaM_freearray(L, f->locvars, f->sizelocvars, struct LocVar);
  luaM_freearray(L, f->k, f->sizek, TObject);
  luaM_freearray(L, f->p, f->sizep, Proto *);
  luaM_freearray(L, f->lineinfo, f->sizelineinfo, int);
  luaM_freelem(L, f, Proto);
}


void luaF_freeclosure (lua_State *L, Closure *c) {
  luaM_free(L, c, sizeclosure(c->nupvalues));
}


/*
** Look for n-th local variable at line `line' in function `func'.
** Returns NULL if not found.
*/
const l_char *luaF_getlocalname (const Proto *f, int local_number, int pc) {
  int i;
  for (i = 0; i<f->sizelocvars && f->locvars[i].startpc <= pc; i++) {
    if (pc < f->locvars[i].endpc) {  /* is variable active? */
      local_number--;
      if (local_number == 0)
        return getstr(f->locvars[i].varname);
    }
  }
  return NULL;  /* not found */
}

