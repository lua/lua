/*
** $Id: ldebug.c,v 1.2 1999/12/23 18:19:57 roberto Exp roberto $
** Debug Interface
** See Copyright Notice in lua.h
*/


#define LUA_REENTRANT

#include "lapi.h"
#include "lauxlib.h"
#include "ldebug.h"
#include "lfunc.h"
#include "lobject.h"
#include "lstate.h"
#include "ltable.h"
#include "ltm.h"
#include "lua.h"
#include "luadebug.h"


static int hasdebuginfo (lua_State *L, lua_Function f) {
  return (f+1 < L->top && (f+1)->ttype == LUA_T_LINE);
}


lua_LHFunction lua_setlinehook (lua_State *L, lua_LHFunction func) {
  lua_LHFunction old = L->linehook;
  L->linehook = func;
  return old;
}


lua_CHFunction lua_setcallhook (lua_State *L, lua_CHFunction func) {
  lua_CHFunction old = L->callhook;
  L->callhook = func;
  return old;
}


int lua_setdebug (lua_State *L, int debug) {
  int old = L->debug;
  L->debug = debug;
  return old;
}


lua_Function lua_stackedfunction (lua_State *L, int level) {
  int i;
  for (i = (L->top-1)-L->stack; i>=0; i--) {
    if (is_T_MARK(L->stack[i].ttype)) {
      if (level == 0)
        return L->stack+i;
      level--;
    }
  }
  return LUA_NOOBJECT;
}


const char *luaG_getname (lua_State *L, const char **name) {
  lua_Function f = lua_stackedfunction(L, 0);
  if (f == LUA_NOOBJECT || !hasdebuginfo(L, f) || ttype(f+2) == LUA_T_NIL)
    return NULL;  /* no name available */
  else {
    int i = (f+2)->value.i;
    if (ttype(f) == LUA_T_LCLMARK)
      f = protovalue(f);
    LUA_ASSERT(L, ttype(f) == LUA_T_LMARK, "must be a Lua function");
    LUA_ASSERT(L, ttype(&tfvalue(f)->consts[i]) == LUA_T_STRING, "");
    *name = tsvalue(&tfvalue(f)->consts[i])->str;
    switch (ttype(f+2)) {
      case LUA_T_NGLOBAL: return "global";
      case LUA_T_NLOCAL: return "local";
      case LUA_T_NDOT: return "field";
      default:
        LUA_INTERNALERROR(L, "invalid tag for NAME");
        return NULL;  /* unreacheable; to avoid warnings */
    }
  }
}


int lua_nups (lua_State *L, lua_Function f) {
  UNUSED(L);
  switch (luaA_normalizedtype(f)) {
    case LUA_T_LCLOSURE:  case LUA_T_CCLOSURE:
      return f->value.cl->nelems;
    default:
      return 0;
  }
}


int lua_currentline (lua_State *L, lua_Function f) {
  return hasdebuginfo(L, f) ? (f+1)->value.i : -1;
}


lua_Object lua_getlocal (lua_State *L, lua_Function f, int local_number,
                         const char **name) {
  /* check whether `f' is a Lua function */
  if (lua_tag(L, f) != LUA_T_LPROTO)
    return LUA_NOOBJECT;
  else {
    TProtoFunc *fp = luaA_protovalue(f)->value.tf;
    *name = luaF_getlocalname(fp, local_number, lua_currentline(L, f));
    if (*name) {
      /* if "*name", there must be a LUA_T_LINE and a NAME */
      /* therefore, f+3 points to function base */
      LUA_ASSERT(L, ttype(f+1) == LUA_T_LINE, "");
      return luaA_putluaObject(L, (f+3)+(local_number-1));
    }
    else
      return LUA_NOOBJECT;
  }
}


int lua_setlocal (lua_State *L, lua_Function f, int local_number) {
  /* check whether `f' is a Lua function */
  if (lua_tag(L, f) != LUA_T_LPROTO)
    return 0;
  else {
    TProtoFunc *fp = luaA_protovalue(f)->value.tf;
    const char *name = luaF_getlocalname(fp, local_number,
                                         lua_currentline(L, f));
    luaA_checkCparams(L, 1);
    --L->top;
    if (name) {
      LUA_ASSERT(L, ttype(f+1) == LUA_T_LINE, "");
      *((f+3)+(local_number-1)) = *L->top;
      return 1;
    }
    else
      return 0;
  }
}


void lua_funcinfo (lua_State *L, lua_Object func,
                   const char **source, int *linedefined) {
  if (!lua_isfunction(L, func))
    lua_error(L, "API error - `funcinfo' called with a non-function value");
  else {
    const TObject *f = luaA_protovalue(func);
    if (luaA_normalizedtype(f) == LUA_T_LPROTO) {
      *source = tfvalue(f)->source->str;
      *linedefined = tfvalue(f)->lineDefined;
    }
    else {
      *source = "(C)";
      *linedefined = -1;
    }
  }
}


static int checkfunc (lua_State *L, TObject *o) {
  return luaO_equalObj(o, L->top);
}


const char *lua_getobjname (lua_State *L, lua_Object o, const char **name) {
  /* try to find a name for given function */
  GlobalVar *g;
  luaA_setnormalized(L->top, o); /* to be used by `checkfunc' */
  for (g=L->rootglobal; g; g=g->next) {
    if (checkfunc(L, &g->value)) {
      *name = g->name->str;
      return "global";
    }
  }
  /* not found: try tag methods */
  if ((*name = luaT_travtagmethods(L, checkfunc)) != NULL)
    return "tag-method";
  else return "";  /* not found at all */
}

static void call_index_error (lua_State *L, TObject *o, const char *tp,
                              const char *v) {
  const char *name;
  const char *kind = luaG_getname(L, &name);
  if (kind) {  /* is there a name? */
    luaL_verror(L, "%.10s `%.30s' is not a %.10s", kind, name, tp);
  }
  else {
    luaL_verror(L, "attempt to %.10s a %.10s value", v, lua_type(L, o));
  }
}


void luaG_callerror (lua_State *L, TObject *func) {
  call_index_error(L, func, "function", "call");
}


void luaG_indexerror (lua_State *L, TObject *t) {
  call_index_error(L, t, "table", "index");
}
