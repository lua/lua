/*
** $Id: lfunc.c,v 1.47 2001/09/07 17:39:10 roberto Exp $
** Auxiliary functions to manipulate prototypes and closures
** See Copyright Notice in lua.h
*/


#include <stdlib.h>

#define LUA_PRIVATE
#include "lua.h"

#include "lfunc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"


#define sizeCclosure(n)	(cast(int, sizeof(CClosure)) + \
                         cast(int, sizeof(TObject)*((n)-1)))

#define sizeLclosure(n)	(cast(int, sizeof(LClosure)) + \
                         cast(int, sizeof(LClosureEntry)*((n)-1)))


Closure *luaF_newCclosure (lua_State *L, int nelems) {
  Closure *c = cast(Closure *, luaM_malloc(L, sizeCclosure(nelems)));
  c->c.isC = 1;
  c->c.next = G(L)->rootcl;
  G(L)->rootcl = c;
  c->c.marked = 0;
  c->c.nupvalues = nelems;
  return c;
}


Closure *luaF_newLclosure (lua_State *L, int nelems) {
  Closure *c = cast(Closure *, luaM_malloc(L, sizeLclosure(nelems)));
  c->l.isC = 0;
  c->l.marked = 0;
  c->l.nupvalues = nelems;
  return c;
}


/*
** returns the open pointer in a closure that points higher into the stack
*/
static StkId uppoint (LClosure *cl) {
  StkId lp = NULL;
  int i;
  for (i=0; i<cl->nupvalues; i++) {
    if (cl->upvals[i].heap == NULL && (lp == NULL || cl->upvals[i].val > lp))
        lp = cl->upvals[i].val;
  }
  return lp;
}


void luaF_LConlist (lua_State *L, Closure *cl) {
  StkId cli = uppoint(&cl->l);
  if (cli == NULL) {  /* no more open entries? */
    cl->l.next = G(L)->rootcl;  /* insert in final list */
    G(L)->rootcl = cl;
  }
  else {  /* insert in list of open closures, ordered by decreasing uppoints */
    Closure **p = &L->opencl;
    while (*p != NULL && uppoint(&(*p)->l) > cli) p = &(*p)->l.next;
    cl->l.next = *p;
    *p = cl;
  }
}


static int closeCl (lua_State *L, LClosure *cl, StkId level) {
  int got = 0;  /* flag: 1 if some pointer in the closure was corrected */
  int i;
  for  (i=0; i<cl->nupvalues; i++) {
    StkId var;
    if (cl->upvals[i].heap == NULL && (var=cl->upvals[i].val) >= level) {
      if (ttype(var) != LUA_TUPVAL) {
        UpVal *v = luaM_new(L, UpVal);
        v->val = *var;
        v->marked = 0;
        v->next = G(L)->rootupval;
        G(L)->rootupval = v;
        setupvalue(var, v);
      }
      cl->upvals[i].heap = vvalue(var);
      cl->upvals[i].val = &vvalue(var)->val;
      got = 1;
    }
  }
  return got;
}


void luaF_close (lua_State *L, StkId level) {
  Closure *affected = NULL;  /* closures with open pointers >= level */
  Closure *cl;
  while ((cl=L->opencl) != NULL) {
    if (!closeCl(L, cast(LClosure *, cl), level)) break;
    /* some pointer in `cl' changed; will re-insert it in original list */
    L->opencl = cl->l.next;  /* remove from original list */
    cl->l.next = affected;
    affected = cl;  /* insert in affected list */
  }
  /* re-insert all affected closures in original list */
  while ((cl=affected) != NULL) {
    affected = cl->l.next;
    luaF_LConlist(L, cl);
  }
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
  luaM_freelem(L, f);
}


void luaF_freeclosure (lua_State *L, Closure *c) {
  int size = (c->c.isC) ? sizeCclosure(c->c.nupvalues) :
                          sizeLclosure(c->l.nupvalues);
  luaM_free(L, c, size);
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

