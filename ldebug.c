/*
** $Id: ldebug.c,v 1.6 2000/01/25 13:57:18 roberto Exp roberto $
** Debug Interface
** See Copyright Notice in lua.h
*/


#define LUA_REENTRANT

#include "lapi.h"
#include "lauxlib.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lobject.h"
#include "lstate.h"
#include "ltable.h"
#include "ltm.h"
#include "lua.h"
#include "luadebug.h"


static const lua_Type normtype[] = {  /* ORDER LUA_T */
  LUA_T_USERDATA, LUA_T_NUMBER, LUA_T_STRING, LUA_T_ARRAY,
  LUA_T_LPROTO, LUA_T_CPROTO, LUA_T_NIL,
  LUA_T_LCLOSURE, LUA_T_CCLOSURE,
  LUA_T_LCLOSURE, LUA_T_CCLOSURE,   /* LUA_T_LCLMARK, LUA_T_CCLMARK */
  LUA_T_LPROTO, LUA_T_CPROTO        /* LUA_T_LMARK, LUA_T_CMARK */
};


static void setnormalized (TObject *d, const TObject *s) {
  d->value = s->value;
  d->ttype = normtype[-ttype(s)];
}



static int hasdebuginfo (lua_State *L, StkId f) {
  return (f+1 < L->top && (f+1)->ttype == LUA_T_LINE);
}


lua_Dbghook lua_setcallhook (lua_State *L, lua_Dbghook func) {
  lua_Dbghook oldhook = L->callhook;
  L->callhook = func;
  return oldhook;
}


lua_Dbghook lua_setlinehook (lua_State *L, lua_Dbghook func) {
  lua_Dbghook oldhook = L->linehook;
  L->linehook = func;
  return oldhook;
}


int lua_setdebug (lua_State *L, int debug) {
  int old = L->debug;
  L->debug = debug;
  return old;
}


static StkId aux_stackedfunction (lua_State *L, int level, StkId top) {
  int i;
  for (i = (top-1)-L->stack; i>=0; i--) {
    if (is_T_MARK(L->stack[i].ttype)) {
      if (level == 0)
        return L->stack+i;
      level--;
    }
  }
  return NULL;
}


int lua_getstack (lua_State *L, int level, lua_Dbgactreg *ar) {
  StkId f = aux_stackedfunction(L, level, L->top);
  if (f == NULL) return 0;  /* there is no such level */
  else {
    ar->_func = f;
    return 1;
  }
}


static const char *luaG_getname (lua_State *L, const char **name, StkId top) {
  StkId f = aux_stackedfunction(L, 0, top);
  if (f == NULL || !hasdebuginfo(L, f) || ttype(f+2) == LUA_T_NIL)
    return "";  /* no name available */
  else {
    int i = (f+2)->value.i;
    if (ttype(f) == LUA_T_LCLMARK)
      f = protovalue(f);
    LUA_ASSERT(L, ttype(f) == LUA_T_LMARK, "must be a Lua function");
    *name = tfvalue(f)->kstr[i]->str;
    return luaO_typename(f+2);
  }
}


static int lua_nups (StkId f) {
  switch (ttype(f)) {
    case LUA_T_LCLOSURE:  case LUA_T_CCLOSURE:
    case LUA_T_LCLMARK:   case LUA_T_CCLMARK:
      return f->value.cl->nelems;
    default:
      return 0;
  }
}


static int lua_currentline (lua_State *L, StkId f) {
  return hasdebuginfo(L, f) ? (f+1)->value.i : -1;
}


static TProtoFunc *getluaproto (StkId f) {
  if (ttype(f) == LUA_T_LMARK)
    return f->value.tf;
  else if (ttype(f) == LUA_T_LCLMARK)
    return protovalue(f)->value.tf;
  else return NULL;
}


int lua_getlocal (lua_State *L, const lua_Dbgactreg *ar, lua_Dbglocvar *v) {
  StkId f = ar->_func;
  TProtoFunc *fp = getluaproto(f);
  if (!fp) return 0;  /* `f' is not a Lua function? */
  v->name = luaF_getlocalname(fp, v->index, lua_currentline(L, f));
  if (!v->name) return 0;
  /* if `name', there must be a LUA_T_LINE and a NAME */
  /* therefore, f+3 points to function base */
  LUA_ASSERT(L, ttype(f+1) == LUA_T_LINE, "");
  v->value = luaA_putluaObject(L, (f+3)+(v->index-1));
  return 1;
}


int lua_setlocal (lua_State *L, const lua_Dbgactreg *ar, lua_Dbglocvar *v) {
  StkId f = ar->_func;
  TProtoFunc *fp = getluaproto(f);
  if (!fp) return 0;  /* `f' is not a Lua function? */
  v->name = luaF_getlocalname(fp, v->index, lua_currentline(L, f));
  if (!v->name) return 0;
  LUA_ASSERT(L, ttype(f+1) == LUA_T_LINE, "");
  *((f+3)+(v->index-1)) = *v->value;
  return 1;
}


static void lua_funcinfo (lua_Dbgactreg *ar) {
  StkId func = ar->_func;
  switch (ttype(func)) {
    case LUA_T_LPROTO:  case LUA_T_LMARK:
      ar->source = tfvalue(func)->source->str;
      ar->linedefined = tfvalue(func)->lineDefined;
      ar->what = "Lua";
      break;
    case LUA_T_LCLOSURE:  case LUA_T_LCLMARK:
      ar->source = tfvalue(protovalue(func))->source->str;
      ar->linedefined = tfvalue(protovalue(func))->lineDefined;
      ar->what = "Lua";
      break;
    default:
      ar->source = "(C)";
      ar->linedefined = -1;
      ar->what = "C";
  }
  if (ar->linedefined == 0)
    ar->what = "main";
}


static int checkfunc (lua_State *L, TObject *o) {
  return luaO_equalObj(o, L->top);
}


static void lua_getobjname (lua_State *L, StkId f, lua_Dbgactreg *ar) {
  GlobalVar *g;
  ar->namewhat = luaG_getname(L, &ar->name, f); /* caller debug information */
    if (*ar->namewhat) return;
  /* try to find a name for given function */
  setnormalized(L->top, f); /* to be used by `checkfunc' */
  for (g=L->rootglobal; g; g=g->next) {
    if (checkfunc(L, &g->value)) {
      ar->name = g->name->str;
      ar->namewhat = "global";
      return;
    }
  }
  /* not found: try tag methods */
  if ((ar->name = luaT_travtagmethods(L, checkfunc)) != NULL)
    ar->namewhat = "tag-method";
  else ar->namewhat = "";  /* not found at all */
}


int lua_getinfo (lua_State *L, const char *what, lua_Dbgactreg *ar) {
  StkId func = ar->_func;
  LUA_ASSERT(L, is_T_MARK(ttype(func)), "invalid activation record");
  for ( ;*what; what++) {
    switch (*what) {
      case 'S':
        lua_funcinfo(ar);
        break;
      case 'l':
        ar->currentline = lua_currentline(L, func);
        break;
      case 'u':
        ar->nups = lua_nups(func);
        break;
      case 'n':
        lua_getobjname(L, func, ar);
        break;
      case 'f':
        setnormalized(L->top, func);
        incr_top;
        ar->func = luaA_putObjectOnTop(L);
        break;
      default: return 0;  /* invalid option */
    }
  }
  return 1;
}



static void call_index_error (lua_State *L, TObject *o, const char *tp,
                              const char *v) {
  const char *name;
  const char *kind = luaG_getname(L, &name, L->top);
  if (*kind) {  /* is there a name? */
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
