/*
** $Id: lfunc.c,v 1.34 2000/10/30 12:20:29 roberto Exp $
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
  int size = sizeclosure(nelems);
  Closure *c = (Closure *)luaM_malloc(L, size);
  c->next = L->rootcl;
  L->rootcl = c;
  c->mark = c;
  c->nupvalues = nelems;
  L->nblocks += size;
  return c;
}


Proto *luaF_newproto (lua_State *L) {
  Proto *f = luaM_new(L, Proto);
  f->knum = NULL;
  f->nknum = 0;
  f->kstr = NULL;
  f->nkstr = 0;
  f->kproto = NULL;
  f->nkproto = 0;
  f->code = NULL;
  f->ncode = 0;
  f->numparams = 0;
  f->is_vararg = 0;
  f->maxstacksize = 0;
  f->marked = 0;
  f->lineinfo = NULL;
  f->nlineinfo = 0;
  f->nlocvars = 0;
  f->locvars = NULL;
  f->lineDefined = 0;
  f->source = NULL;
  f->next = L->rootproto;  /* chain in list of protos */
  L->rootproto = f;
  return f;
}


static size_t protosize (Proto *f) {
  return sizeof(Proto)
       + f->nknum*sizeof(Number)
       + f->nkstr*sizeof(TString *)
       + f->nkproto*sizeof(Proto *)
       + f->ncode*sizeof(Instruction)
       + f->nlocvars*sizeof(struct LocVar)
       + f->nlineinfo*sizeof(int);
}


void luaF_protook (lua_State *L, Proto *f, int pc) {
  f->ncode = pc;  /* signal that proto was properly created */
  L->nblocks += protosize(f);
}


void luaF_freeproto (lua_State *L, Proto *f) {
  if (f->ncode > 0)  /* function was properly created? */
    L->nblocks -= protosize(f);
  luaM_free(L, f->code);
  luaM_free(L, f->locvars);
  luaM_free(L, f->kstr);
  luaM_free(L, f->knum);
  luaM_free(L, f->kproto);
  luaM_free(L, f->lineinfo);
  luaM_free(L, f);
}


void luaF_freeclosure (lua_State *L, Closure *c) {
  L->nblocks -= sizeclosure(c->nupvalues);
  luaM_free(L, c);
}


/*
** Look for n-th local variable at line `line' in function `func'.
** Returns NULL if not found.
*/
const char *luaF_getlocalname (const Proto *f, int local_number, int pc) {
  int i;
  for (i = 0; i<f->nlocvars && f->locvars[i].startpc <= pc; i++) {
    if (pc < f->locvars[i].endpc) {  /* is variable active? */
      local_number--;
      if (local_number == 0)
        return f->locvars[i].varname->str;
    }
  }
  return NULL;  /* not found */
}

