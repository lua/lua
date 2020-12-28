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



CClosure *luaF_newCclosure (lua_State *L, int nupvals) {
  GCObject *o = luaC_newobj(L, LUA_VCCL, sizeCclosure(nupvals));
  CClosure *c = gco2ccl(o);
  c->nupvalues = cast_byte(nupvals);
  return c;
}


LClosure *luaF_newLclosure (lua_State *L, int nupvals) {
  GCObject *o = luaC_newobj(L, LUA_VLCL, sizeLclosure(nupvals));
  LClosure *c = gco2lcl(o);
  c->p = NULL;
  c->nupvalues = cast_byte(nupvals);
  while (nupvals--) c->upvals[nupvals] = NULL;
  return c;
}


/*
** fill a closure with new closed upvalues
*/
void luaF_initupvals (lua_State *L, LClosure *cl) {
  int i;
  for (i = 0; i < cl->nupvalues; i++) {
    GCObject *o = luaC_newobj(L, LUA_VUPVAL, sizeof(UpVal));
    UpVal *uv = gco2upv(o);
    uv->v = &uv->u.value;  /* make it closed */
    setnilvalue(uv->v);
    cl->upvals[i] = uv;
    luaC_objbarrier(L, cl, uv);
  }
}


/*
** Create a new upvalue at the given level, and link it to the list of
** open upvalues of 'L' after entry 'prev'.
**/
static UpVal *newupval (lua_State *L, int tbc, StkId level, UpVal **prev) {
  GCObject *o = luaC_newobj(L, LUA_VUPVAL, sizeof(UpVal));
  UpVal *uv = gco2upv(o);
  UpVal *next = *prev;
  uv->v = s2v(level);  /* current value lives in the stack */
  uv->tbc = tbc;
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
** Find and reuse, or create if it does not exist, an upvalue
** at the given level.
*/
UpVal *luaF_findupval (lua_State *L, StkId level) {
  UpVal **pp = &L->openupval;
  UpVal *p;
  lua_assert(isintwups(L) || L->openupval == NULL);
  while ((p = *pp) != NULL && uplevel(p) >= level) {  /* search for it */
    lua_assert(!isdead(G(L), p));
    if (uplevel(p) == level)  /* corresponding upvalue? */
      return p;  /* return it */
    pp = &p->u.open.next;
  }
  /* not found: create a new upvalue after 'pp' */
  return newupval(L, 0, level, pp);
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
** Raise an error with message 'msg', inserting the name of the
** local variable at position 'level' in the stack.
*/
static void varerror (lua_State *L, StkId level, const char *msg) {
  int idx = cast_int(level - L->ci->func);
  const char *vname = luaG_findlocal(L, L->ci, idx, NULL);
  if (vname == NULL) vname = "?";
  luaG_runerror(L, msg, vname);
}


/*
** Prepare and call a closing method.
** If status is CLOSEKTOP, the call to the closing method will be pushed
** at the top of the stack. Otherwise, values can be pushed right after
** the 'level' of the upvalue being closed, as everything after that
** won't be used again.
*/
static void callclosemth (lua_State *L, StkId level, int status) {
  TValue *uv = s2v(level);  /* value being closed */
  TValue *errobj;
  if (status == CLOSEKTOP)
    errobj = &G(L)->nilvalue;  /* error object is nil */
  else {  /* 'luaD_seterrorobj' will set top to level + 2 */
    errobj = s2v(level + 1);  /* error object goes after 'uv' */
    luaD_seterrorobj(L, status, level + 1);  /* set error object */
  }
  if (prepclosingmethod(L, uv, errobj))  /* something to call? */
    luaD_callnoyield(L, L->top - 3, 0);  /* call method */
  else if (!l_isfalse(uv))  /* non-closable non-false value? */
    varerror(L, level, "attempt to close non-closable variable '%s'");
}


/*
** Try to create a to-be-closed upvalue
** (can raise a memory-allocation error)
*/
static void trynewtbcupval (lua_State *L, void *ud) {
  newupval(L, 1, cast(StkId, ud), &L->openupval);
}


/*
** Create a to-be-closed upvalue. If there is a memory error
** when creating the upvalue, the closing method must be called here,
** as there is no upvalue to call it later.
*/
void luaF_newtbcupval (lua_State *L, StkId level) {
  TValue *obj = s2v(level);
  lua_assert(L->openupval == NULL || uplevel(L->openupval) < level);
  if (!l_isfalse(obj)) {  /* false doesn't need to be closed */
    int status;
    const TValue *tm = luaT_gettmbyobj(L, obj, TM_CLOSE);
    if (ttisnil(tm))  /* no metamethod? */
      varerror(L, level, "variable '%s' got a non-closable value");
    status = luaD_rawrunprotected(L, trynewtbcupval, level);
    if (unlikely(status != LUA_OK)) {  /* memory error creating upvalue? */
      lua_assert(status == LUA_ERRMEM);
      luaD_seterrorobj(L, LUA_ERRMEM, level + 1);  /* save error message */
      /* next call must succeed, as object is closable */
      prepclosingmethod(L, s2v(level), s2v(level + 1));
      luaD_callnoyield(L, L->top - 3, 0);  /* call method */
      luaD_throw(L, LUA_ERRMEM);  /* throw memory error */
    }
  }
}


void luaF_unlinkupval (UpVal *uv) {
  lua_assert(upisopen(uv));
  *uv->u.open.previous = uv->u.open.next;
  if (uv->u.open.next)
    uv->u.open.next->u.open.previous = uv->u.open.previous;
}


/*
** Close all upvalues up to the given stack level. A 'status' equal
** to NOCLOSINGMETH closes upvalues without running any __close
** metamethods.
*/
void luaF_close (lua_State *L, StkId level, int status) {
  UpVal *uv;
  StkId upl;  /* stack index pointed by 'uv' */
  while ((uv = L->openupval) != NULL && (upl = uplevel(uv)) >= level) {
    TValue *slot = &uv->u.value;  /* new position for value */
    lua_assert(uplevel(uv) < L->top);
    luaF_unlinkupval(uv);  /* remove upvalue from 'openupval' list */
    setobj(L, slot, uv->v);  /* move value to upvalue slot */
    uv->v = slot;  /* now current value lives here */
    if (!iswhite(uv)) {  /* neither white nor dead? */
      nw2black(uv);  /* closed upvalues cannot be gray */
      luaC_barrier(L, uv, slot);
    }
    if (uv->tbc && status != NOCLOSINGMETH) {
      ptrdiff_t levelrel = savestack(L, level);
      callclosemth(L, upl, status);  /* may change the stack */
      level = restorestack(L, levelrel);
    }
  }
}


Proto *luaF_newproto (lua_State *L) {
  GCObject *o = luaC_newobj(L, LUA_VPROTO, sizeof(Proto));
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

