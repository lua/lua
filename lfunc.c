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

#include "ldebug.h"
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


/*
** Create a new upvalue with the given tag at the given level,
** and link it to the list of open upvalues of 'L' after entry 'prev'.
**/
static UpVal *newupval (lua_State *L, int tag, StkId level, UpVal **prev) {
  GCObject *o = luaC_newobj(L, tag, sizeof(UpVal));
  UpVal *uv = gco2upv(o);
  UpVal *next = *prev;
  uv->v = s2v(level);  /* current value lives in the stack */
  uv->u.open.next = next;  /* link it to list of open upvalues */
  uv->u.open.previous = prev;
  if (next)
    next->u.open.previous = &uv->u.open.next;
  *prev = uv;
  if (!isintwups(L)) {  /* thread not in list of threads with upvalues? */
    L->twups = G(L)->twups;  /* link it to the list */
    G(L)->twups = L;
  }
  return uv;
}


/*
** Find and reuse, or create if it does not exist, a regular upvalue
** at the given level.
*/
UpVal *luaF_findupval (lua_State *L, StkId level) {
  UpVal **pp = &L->openupval;
  UpVal *p;
  lua_assert(isintwups(L) || L->openupval == NULL);
  while ((p = *pp) != NULL && uplevel(p) >= level) {  /* search for it */
    if (uplevel(p) == level && !isdead(G(L), p))  /* corresponding upvalue? */
      return p;  /* return it */
    pp = &p->u.open.next;
  }
  /* not found: create a new upvalue after 'pp' */
  return newupval(L, LUA_TUPVAL, level, pp);
}


static void callclose (lua_State *L, void *ud) {
  UNUSED(ud);
  luaD_callnoyield(L, L->top - 3, 0);
}


/*
** Prepare closing method plus its arguments for object 'obj' with
** error message 'err'. (This function assumes EXTRA_STACK.)
*/
static int prepclosingmethod (lua_State *L, TValue *obj, TValue *err) {
  StkId top = L->top;
  const TValue *tm = luaT_gettmbyobj(L, obj, TM_CLOSE);
  if (ttisnil(tm))  /* no metamethod? */
    return 0;  /* nothing to call */
  setobj2s(L, top, tm);  /* will call metamethod... */
  setobj2s(L, top + 1, obj);  /* with 'self' as the 1st argument */
  setobj2s(L, top + 2, err);  /* and error msg. as 2nd argument */
  L->top = top + 3;  /* add function and arguments */
  return 1;
}


/*
** Prepare and call a closing method. If status is OK, code is still
** inside the original protected call, and so any error will be handled
** there. Otherwise, a previous error already activated original
** protected call, and so the call to the closing method must be
** protected here. (A status = CLOSEPROTECT behaves like a previous
** error, to also run the closing method in protected mode).
** If status is OK, the call to the closing method will be pushed
** at the top of the stack. Otherwise, values are pushed after
** the 'level' of the upvalue being closed, as everything after
** that won't be used again.
*/
static int callclosemth (lua_State *L, TValue *uv, StkId level, int status) {
  if (likely(status == LUA_OK)) {
    if (prepclosingmethod(L, uv, &G(L)->nilvalue))  /* something to call? */
      callclose(L, NULL);  /* call closing method */
    else if (!ttisnil(uv)) {  /* non-closable non-nil value? */
      int idx = cast_int(level - L->ci->func);
      const char *vname = luaG_findlocal(L, L->ci, idx, NULL);
      if (vname == NULL) vname = "?";
      luaG_runerror(L, "attempt to close non-closable variable '%s'", vname);
    }
  }
  else {  /* there was an error */
    /* save error message and set stack top to 'level + 1' */
    luaD_seterrorobj(L, status, level);
    if (prepclosingmethod(L, uv, s2v(level))) {  /* something to call? */
      int newstatus = luaD_pcall(L, callclose, NULL, savestack(L, level), 0);
      if (newstatus != LUA_OK)  /* another error when closing? */
        status = newstatus;  /* this will be the new error */
    }
    /* else no metamethod; ignore this case and keep original error */
  }
  return status;
}


/*
** Try to create a to-be-closed upvalue
** (can raise a memory-allocation error)
*/
static void trynewtbcupval (lua_State *L, void *ud) {
  StkId level = cast(StkId, ud);
  lua_assert(L->openupval == NULL || uplevel(L->openupval) < level);
  newupval(L, LUA_TUPVALTBC, level, &L->openupval);
}


/*
** Create a to-be-closed upvalue. If there is a memory error
** when creating the upvalue, the closing method must be called here,
** as there is no upvalue to call it later.
*/
void luaF_newtbcupval (lua_State *L, StkId level) {
  int status = luaD_rawrunprotected(L, trynewtbcupval, level);
  if (unlikely(status != LUA_OK)) {  /* memory error creating upvalue? */
    lua_assert(status == LUA_ERRMEM);
    luaD_seterrorobj(L, LUA_ERRMEM, level + 1);  /* save error message */
    if (prepclosingmethod(L, s2v(level), s2v(level + 1)))
      callclose(L, NULL);  /* call closing method */
    luaD_throw(L, LUA_ERRMEM);  /* throw memory error */
  }
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
    if (uv->tt == LUA_TUPVALTBC && status != NOCLOSINGMETH) {
      /* must run closing method */
      ptrdiff_t levelrel = savestack(L, level);
      status = callclosemth(L, uv->v, upl, status);  /* may change the stack */
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

