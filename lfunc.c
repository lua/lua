/*
** $Id: lfunc.c,v 1.37 2001/01/19 13:20:30 roberto Exp roberto $
** Auxiliary functions to manipulate prototypes and closures
** See Copyright Notice in lua.h
*/


#include <stdlib.h>

#include "lua.h"

#include "lfunc.h"
#include "lmem.h"
#include "lstate.h"


#define sizeclosure(n)	((int)sizeof(Closure) + (int)sizeof(TObject)*((n)-1))


Closure *luaF_newclosure (lua_State *L, int nelems) {
  Closure *c = (Closure *)luaM_malloc(L, sizeclosure(nelems));
  c->v.ttype = LUA_TFUNCTION;
  c->next = G(L)->rootcl;
  G(L)->rootcl = c;
  c->mark = c;
  c->nupvalues = nelems;
  return c;
}


Proto *luaF_newproto (lua_State *L) {
  Proto *f = luaM_new(L, Proto);
  f->knum = NULL;
  f->sizeknum = 0;
  f->kstr = NULL;
  f->sizekstr = 0;
  f->kproto = NULL;
  f->sizekproto = 0;
  f->code = NULL;
  f->sizecode = 0;
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
  luaM_freearray(L, f->kstr, f->sizekstr, TString *);
  luaM_freearray(L, f->knum, f->sizeknum, lua_Number);
  luaM_freearray(L, f->kproto, f->sizekproto, Proto *);
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
const char *luaF_getlocalname (const Proto *f, int local_number, int pc) {
  int i;
  for (i = 0; i<f->sizelocvars && f->locvars[i].startpc <= pc; i++) {
    if (pc < f->locvars[i].endpc) {  /* is variable active? */
      local_number--;
      if (local_number == 0)
        return f->locvars[i].varname->str;
    }
  }
  return NULL;  /* not found */
}

