/*
** $Id: lfunc.c $
** Auxiliary functions to manipulate prototypes and closures
** See Copyright Notice in lua.h
*/

#define lfunc_c
#define LUA_CORE

#include "lprefix.h"


#include <stddef.h>

#include "lua.h"

#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"



CClosure *luaF_newCclosure (lua_State *L, int n) {
  GCObject *o = luaC_newobj(L, LUA_TCCL, sizeCclosure(n));
  CClosure *c = gco2ccl(o);
  c->nupvalues = cast_byte(n);
  return c;
}


LClosure *luaF_newLclosure (lua_State *L, int n) {
  GCObject *o = luaC_newobj(L, LUA_TLCL, sizeLclosure(n));
  LClosure *c = gco2lcl(o);
  c->p = NULL;
  c->nupvalues = cast_byte(n);
  while (n--) c->upvals[n] = NULL;
  return c;
}

/*
** fill a closure with new closed upvalues
*/
void luaF_initupvals (lua_State *L, LClosure *cl) {
  int i;
  for (i = 0; i < cl->nupvalues; i++) {
    GCObject *o = luaC_newobj(L, LUA_TUPVAL, sizeof(UpVal));
    UpVal *uv = gco2upv(o);
    uv->v = &uv->u.value;  /* make it closed */
    setnilvalue(uv->v);
    cl->upvals[i] = uv;
    luaC_objbarrier(L, cl, o);
  }
}


UpVal *luaF_findupval (lua_State *L, StkId level) {
  UpVal **pp = &L->openupval;
  GCObject *o;
  UpVal *p;
  UpVal *uv;
  lua_assert(isintwups(L) || L->openupval == NULL);
  while ((p = *pp) != NULL && uplevel(p) >= level) {
    if (uplevel(p) == level && !isdead(G(L), p))  /* corresponding upvalue? */
      return p;  /* return it */
    pp = &p->u.open.next;
  }
  /* not found: create a new upvalue between 'pp' and 'p' */
  o = luaC_newobj(L, LUA_TUPVAL, sizeof(UpVal));
  uv = gco2upv(o);
  uv->u.open.next = p;  /* link it to list of open upvalues */
  uv->u.open.previous = pp;
  if (p)
    p->u.open.previous = &uv->u.open.next;
  *pp = uv;
  uv->v = s2v(level);  /* current value lives in the stack */
  if (!isintwups(L)) {  /* thread not in list of threads with upvalues? */
    L->twups = G(L)->twups;  /* link it to the list */
    G(L)->twups = L;
  }
  return uv;
}


static void callclose (lua_State *L, void *ud) {
  luaD_callnoyield(L, cast(StkId, ud), 0);
}


static int closeupval (lua_State *L, UpVal *uv, StkId level, int status) {
  StkId func = level + 1;  /* save slot for old error message */
  if (status != LUA_OK)  /* was there an error? */
    luaD_seterrorobj(L, status, level);  /* save error message */
  else
    setnilvalue(s2v(level));
  if (ttisfunction(uv->v)) {  /* object to-be-closed is a function? */
    setobj2s(L, func, uv->v);  /* will call it */
    setobjs2s(L, func + 1, level);  /* error msg. as argument */
  }
  else {  /* try '__close' metamethod */
    const TValue *tm = luaT_gettmbyobj(L, uv->v, TM_CLOSE);
    if (ttisnil(tm))
      return status;  /* no metamethod */
    setobj2s(L, func, tm);  /* will call metamethod */
    setobj2s(L, func + 1, uv->v);  /* with 'self' as argument */
  }
  L->top = func + 2;  /* add function and argument */
  if (status == LUA_OK)  /* not in "error mode"? */
    callclose(L, func);  /* call closing method */
  else {  /* already inside error handler; cannot raise another error */
    int newstatus = luaD_pcall(L, callclose, func, savestack(L, level), 0);
    if (newstatus != LUA_OK)  /* error when closing? */
      status = newstatus;  /* this will be the new error */
  }
  return status;
}


void luaF_unlinkupval (UpVal *uv) {
  lua_assert(upisopen(uv));
  *uv->u.open.previous = uv->u.open.next;
  if (uv->u.open.next)
    uv->u.open.next->u.open.previous = uv->u.open.previous;
}


int luaF_close (lua_State *L, StkId level, int status) {
  UpVal *uv;
  while ((uv = L->openupval) != NULL && uplevel(uv) >= level) {
    StkId upl = uplevel(uv);
    TValue *slot = &uv->u.value;  /* new position for value */
    luaF_unlinkupval(uv);
    setobj(L, slot, uv->v);  /* move value to upvalue slot */
    uv->v = slot;  /* now current value lives here */
    if (!iswhite(uv))
      gray2black(uv);  /* closed upvalues cannot be gray */
    luaC_barrier(L, uv, slot);
    if (status >= 0 && uv->tt == LUA_TUPVALTBC) {  /* must be closed? */
      ptrdiff_t levelrel = savestack(L, level);
      status = closeupval(L, uv, upl, status);  /* may reallocate the stack */
      level = restorestack(L, levelrel);
    }
  }
  return status;
}


Proto *luaF_newproto (lua_State *L) {
  GCObject *o = luaC_newobj(L, LUA_TPROTO, sizeof(Proto));
  Proto *f = gco2p(o);
  f->k = NULL;
  f->sizek = 0;
  f->p = NULL;
  f->sizep = 0;
  f->code = NULL;
  f->cache = NULL;
  f->cachemiss = 0;
  f->sizecode = 0;
  f->lineinfo = NULL;
  f->sizelineinfo = 0;
  f->abslineinfo = NULL;
  f->sizeabslineinfo = 0;
  f->upvalues = NULL;
  f->sizeupvalues = 0;
  f->numparams = 0;
  f->is_vararg = 0;
  f->maxstacksize = 0;
  f->locvars = NULL;
  f->sizelocvars = 0;
  f->linedefined = 0;
  f->lastlinedefined = 0;
  f->source = NULL;
  return f;
}


void luaF_freeproto (lua_State *L, Proto *f) {
  luaM_freearray(L, f->code, f->sizecode);
  luaM_freearray(L, f->p, f->sizep);
  luaM_freearray(L, f->k, f->sizek);
  luaM_freearray(L, f->lineinfo, f->sizelineinfo);
  luaM_freearray(L, f->abslineinfo, f->sizeabslineinfo);
  luaM_freearray(L, f->locvars, f->sizelocvars);
  luaM_freearray(L, f->upvalues, f->sizeupvalues);
  luaM_free(L, f);
}


/*
** Look for n-th local variable at line 'line' in function 'func'.
** Returns NULL if not found.
*/
const char *luaF_getlocalname (const Proto *f, int local_number, int pc) {
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

