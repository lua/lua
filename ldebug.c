/*
** $Id: ldebug.c,v 1.1 1999/12/14 18:31:20 roberto Exp roberto $
** Debug Interface
** See Copyright Notice in lua.h
*/


#define LUA_REENTRANT

#include "lapi.h"
#include "lfunc.h"
#include "lobject.h"
#include "lstate.h"
#include "ltable.h"
#include "ltm.h"
#include "lua.h"
#include "luadebug.h"



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
  return (f+1 < L->top && (f+1)->ttype == LUA_T_LINE) ? (f+1)->value.i : -1;
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
      /* if "*name", there must be a LUA_T_LINE */
      /* therefore, f+2 points to function base */
      return luaA_putluaObject(L, (f+2)+(local_number-1));
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
      /* if "name", there must be a LUA_T_LINE */
      /* therefore, f+2 points to function base */
      *((f+2)+(local_number-1)) = *L->top;
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

