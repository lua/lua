/*
** $Id: lapi.c,v 1.61 1999/12/01 19:50:08 roberto Exp roberto $
** Lua API
** See Copyright Notice in lua.h
*/


#include <string.h>

#define LUA_REENTRANT

#include "lapi.h"
#include "lauxlib.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lref.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lua.h"
#include "luadebug.h"
#include "lvm.h"


const char lua_ident[] = "$Lua: " LUA_VERSION " " LUA_COPYRIGHT " $\n"
                               "$Authors:  " LUA_AUTHORS " $";



static lua_Type normalized_type (const TObject *o) {
  int t = ttype(o);
  switch (t) {
    case LUA_T_PMARK:
      return LUA_T_PROTO;
    case LUA_T_CMARK:
      return LUA_T_CPROTO;
    case LUA_T_CLMARK:
      return LUA_T_CLOSURE;
    default:
      return t;
  }
}


static void set_normalized (TObject *d, const TObject *s) {
  d->value = s->value;
  d->ttype = normalized_type(s);
}


static const TObject *luaA_protovalue (const TObject *o) {
  return (normalized_type(o) == LUA_T_CLOSURE) ?  protovalue(o) : o;
}


static void checkCparams (lua_State *L, int nParams) {
  if (nParams > L->top-L->Cstack.base)
    lua_error(L, "API error - wrong number of arguments in C2lua stack");
}


static lua_Object put_luaObject (lua_State *L, const TObject *o) {
  luaD_openstack(L, L->Cstack.base);
  *L->Cstack.base++ = *o;
  return L->Cstack.base-1;
}


lua_Object luaA_putObjectOnTop (lua_State *L) {
  luaD_openstack(L, L->Cstack.base);
  *L->Cstack.base++ = *(--L->top);
  return L->Cstack.base-1;
}


static void top2LC (lua_State *L, int n) {
  /* Put the 'n' elements on the top as the Lua2C contents */
  L->Cstack.base = L->top;  /* new base */
  L->Cstack.lua2C = L->Cstack.base-n;  /* position of the new results */
  L->Cstack.num = n;  /* number of results */
}


lua_Object lua_pop (lua_State *L) {
  checkCparams(L, 1);
  return luaA_putObjectOnTop(L);
}


/*
** Get a parameter, returning the object handle or LUA_NOOBJECT on error.
** `number' must be 1 to get the first parameter.
*/
lua_Object lua_lua2C (lua_State *L, int number) {
  if (number <= 0 || number > L->Cstack.num) return LUA_NOOBJECT;
  return L->Cstack.lua2C+number-1;
}


int lua_callfunction (lua_State *L, lua_Object function) {
  if (function == LUA_NOOBJECT)
    return 1;
  else {
    luaD_openstack(L, L->Cstack.base);
    set_normalized(L->Cstack.base, function);
    return luaD_protectedrun(L);
  }
}


lua_Object lua_gettagmethod (lua_State *L, int tag, const char *event) {
  return put_luaObject(L, luaT_gettagmethod(L, tag, event));
}


lua_Object lua_settagmethod (lua_State *L, int tag, const char *event) {
  checkCparams(L, 1);
  luaT_settagmethod(L, tag, event, L->top-1);
  return luaA_putObjectOnTop(L);
}


lua_Object lua_seterrormethod (lua_State *L) {
  lua_Object temp;
  checkCparams(L, 1);
  temp = lua_getglobal(L, "_ERRORMESSAGE");
  lua_setglobal(L, "_ERRORMESSAGE");
  return temp;
}


lua_Object lua_gettable (lua_State *L) {
  checkCparams(L, 2);
  luaV_gettable(L);
  return luaA_putObjectOnTop(L);
}


lua_Object lua_rawgettable (lua_State *L) {
  checkCparams(L, 2);
  if (ttype(L->top-2) != LUA_T_ARRAY)
    lua_error(L, "indexed expression not a table in rawgettable");
  *(L->top-2) = *luaH_get(L, avalue(L->top-2), L->top-1);
  --L->top;
  return luaA_putObjectOnTop(L);
}


void lua_settable (lua_State *L) {
  checkCparams(L, 3);
  luaV_settable(L, L->top-3);
  L->top -= 2;  /* pop table and index */
}


void lua_rawsettable (lua_State *L) {
  checkCparams(L, 3);
  luaV_rawsettable(L, L->top-3);
}


lua_Object lua_createtable (lua_State *L) {
  TObject o;
  luaC_checkGC(L);
  avalue(&o) = luaH_new(L, 0);
  ttype(&o) = LUA_T_ARRAY;
  return put_luaObject(L, &o);
}


lua_Object lua_getglobal (lua_State *L, const char *name) {
  luaD_checkstack(L, 2);  /* may need that to call T.M. */
  luaV_getglobal(L, luaS_assertglobalbyname(L, name));
  return luaA_putObjectOnTop(L);
}


lua_Object lua_rawgetglobal (lua_State *L, const char *name) {
  GlobalVar *gv = luaS_assertglobalbyname(L, name);
  return put_luaObject(L, &gv->value);
}


void lua_setglobal (lua_State *L, const char *name) {
  checkCparams(L, 1);
  luaD_checkstack(L, 2);  /* may need that to call T.M. */
  luaV_setglobal(L, luaS_assertglobalbyname(L, name));
}


void lua_rawsetglobal (lua_State *L, const char *name) {
  GlobalVar *gv = luaS_assertglobalbyname(L, name);
  checkCparams(L, 1);
  gv->value = *(--L->top);
}


const char *lua_type (lua_State *L, lua_Object o) {
  UNUSED(L);
  return (o == LUA_NOOBJECT) ? "NOOBJECT" : luaO_typename(L, o);
}

int lua_isnil (lua_State *L, lua_Object o) {
  UNUSED(L);
  return (o != LUA_NOOBJECT) && (ttype(o) == LUA_T_NIL);
}

int lua_istable (lua_State *L, lua_Object o) {
  UNUSED(L);
  return (o != LUA_NOOBJECT) && (ttype(o) == LUA_T_ARRAY);
}

int lua_isuserdata (lua_State *L, lua_Object o) {
  UNUSED(L);
  return (o != LUA_NOOBJECT) && (ttype(o) == LUA_T_USERDATA);
}

int lua_iscfunction (lua_State *L, lua_Object o) {
  UNUSED(L);
  return (lua_tag(L, o) == LUA_T_CPROTO);
}

int lua_isnumber (lua_State *L, lua_Object o) {
  UNUSED(L);
  return (o != LUA_NOOBJECT) && (tonumber(o) == 0);
}

int lua_isstring (lua_State *L, lua_Object o) {
  int t = lua_tag(L, o);
  return (t == LUA_T_STRING) || (t == LUA_T_NUMBER);
}

int lua_isfunction (lua_State *L, lua_Object o) {
  int t = lua_tag(L, o);
  return (t == LUA_T_PROTO) || (t == LUA_T_CPROTO);
}

int lua_equal(lua_State *L, lua_Object o1, lua_Object o2) {
  UNUSED(L);
  if (o1 == LUA_NOOBJECT || o2 == LUA_NOOBJECT) return (o1 == o2);
  else return luaO_equalObj(o1, o2);
}


double lua_getnumber (lua_State *L, lua_Object obj) {
  UNUSED(L);
  if (obj == LUA_NOOBJECT) return 0.0;
  if (tonumber(obj)) return 0.0;
  else return (nvalue(obj));
}

const char *lua_getstring (lua_State *L, lua_Object obj) {
  luaC_checkGC(L);  /* `tostring' may create a new string */
  if (obj == LUA_NOOBJECT || tostring(L, obj))
    return NULL;
  else return (svalue(obj));
}

long lua_strlen (lua_State *L, lua_Object obj) {
  UNUSED(L);
  if (obj == LUA_NOOBJECT || tostring(L, obj))
    return 0L;
  else return (tsvalue(obj)->u.s.len);
}

void *lua_getuserdata (lua_State *L, lua_Object obj) {
  UNUSED(L);
  if (obj == LUA_NOOBJECT || ttype(obj) != LUA_T_USERDATA)
    return NULL;
  else return tsvalue(obj)->u.d.value;
}

lua_CFunction lua_getcfunction (lua_State *L, lua_Object obj) {
  if (!lua_iscfunction(L, obj))
    return NULL;
  else return fvalue(luaA_protovalue(obj));
}


void lua_pushnil (lua_State *L) {
  ttype(L->top) = LUA_T_NIL;
  incr_top;
}

void lua_pushnumber (lua_State *L, double n) {
  ttype(L->top) = LUA_T_NUMBER;
  nvalue(L->top) = n;
  incr_top;
}

void lua_pushlstring (lua_State *L, const char *s, long len) {
  tsvalue(L->top) = luaS_newlstr(L, s, len);
  ttype(L->top) = LUA_T_STRING;
  incr_top;
  luaC_checkGC(L);
}

void lua_pushstring (lua_State *L, const char *s) {
  if (s == NULL)
    lua_pushnil(L);
  else
    lua_pushlstring(L, s, strlen(s));
}

void lua_pushcclosure (lua_State *L, lua_CFunction fn, int n) {
  if (fn == NULL)
    lua_error(L, "API error - attempt to push a NULL Cfunction");
  checkCparams(L, n);
  ttype(L->top) = LUA_T_CPROTO;
  fvalue(L->top) = fn;
  incr_top;
  luaV_closure(L, n);
  luaC_checkGC(L);
}

void lua_pushusertag (lua_State *L, void *u, int tag) {
  if (tag < 0 && tag != LUA_ANYTAG)
    luaT_realtag(L, tag);  /* error if tag is not valid */
  tsvalue(L->top) = luaS_createudata(L, u, tag);
  ttype(L->top) = LUA_T_USERDATA;
  incr_top;
  luaC_checkGC(L);
}

void luaA_pushobject (lua_State *L, const TObject *o) {
  *L->top = *o;
  incr_top;
}

void lua_pushobject (lua_State *L, lua_Object o) {
  if (o == LUA_NOOBJECT)
    lua_error(L, "API error - attempt to push a NOOBJECT");
  set_normalized(L->top, o);
  incr_top;
}


int lua_tag (lua_State *L, lua_Object o) {
  UNUSED(L);
  if (o == LUA_NOOBJECT)
     return LUA_T_NIL;
  else {
    int t;
    switch (t = ttype(o)) {
      case LUA_T_USERDATA:
        return o->value.ts->u.d.tag;
      case LUA_T_ARRAY:
        return o->value.a->htag;
      case LUA_T_PMARK:
        return LUA_T_PROTO;
      case LUA_T_CMARK:
        return LUA_T_CPROTO;
      case LUA_T_CLOSURE: case LUA_T_CLMARK:
        return o->value.cl->consts[0].ttype;
#ifdef DEBUG
      case LUA_T_LINE:
        LUA_INTERNALERROR(L, "invalid type");
#endif
      default:
        return t;
    }
  }
}


void lua_settag (lua_State *L, int tag) {
  checkCparams(L, 1);
  luaT_realtag(L, tag);
  switch (ttype(L->top-1)) {
    case LUA_T_ARRAY:
      (L->top-1)->value.a->htag = tag;
      break;
    case LUA_T_USERDATA:
      (L->top-1)->value.ts->u.d.tag = tag;
      break;
    default:
      luaL_verror(L, "cannot change the tag of a %.20s",
                  luaO_typename(L, L->top-1));
  }
  L->top--;
}


GlobalVar *luaA_nextvar (lua_State *L, TaggedString *ts) {
  GlobalVar *gv;
  if (ts == NULL)
    gv = L->rootglobal;  /* first variable */
  else {
    /* check whether name is in global var list */
    luaL_arg_check(L, ts->u.s.gv, 1, "variable name expected");
    gv = ts->u.s.gv->next;  /* get next */
  }
  while (gv && gv->value.ttype == LUA_T_NIL)  /* skip globals with nil */
    gv = gv->next;
  if (gv) {
    ttype(L->top) = LUA_T_STRING; tsvalue(L->top) = gv->name;
    incr_top;
    luaA_pushobject(L, &gv->value);
  }
  return gv;
}


const char *lua_nextvar (lua_State *L, const char *varname) {
  TaggedString *ts = (varname == NULL) ? NULL : luaS_new(L, varname);
  GlobalVar *gv = luaA_nextvar(L, ts);
  if (gv) {
    top2LC(L, 2);
    return gv->name->str;
  }
  else {
    top2LC(L, 0);
    return NULL;
  }
}


int luaA_next (lua_State *L, const Hash *t, int i) {
  int tsize = t->size;
  for (; i<tsize; i++) {
    Node *n = node(t, i);
    if (ttype(val(n)) != LUA_T_NIL) {
      luaA_pushobject(L, key(n));
      luaA_pushobject(L, val(n));
      return i+1;  /* index to be used next time */
    }
  }
  return 0;  /* no more elements */
}


int lua_next (lua_State *L, lua_Object t, int i) {
  if (ttype(t) != LUA_T_ARRAY)
    lua_error(L, "API error - object is not a table in `lua_next'"); 
  i = luaA_next(L, avalue(t), i);
  top2LC(L, (i==0) ? 0 : 2);
  return i;
}



/*
** {======================================================
** To manipulate some state information
** =======================================================
*/



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

/* }====================================================== */


/*
** {======================================================
** Debug interface
** =======================================================
*/


lua_Function lua_stackedfunction (lua_State *L, int level) {
  int i;
  for (i = (L->top-1)-L->stack; i>=0; i--) {
    int t = L->stack[i].ttype;
    if (t == LUA_T_CLMARK || t == LUA_T_PMARK || t == LUA_T_CMARK)
      if (level-- == 0)
        return L->stack+i;
  }
  return LUA_NOOBJECT;
}


int lua_nups (lua_State *L, lua_Function f) {
  UNUSED(L);
  return (!f || normalized_type(f) != LUA_T_CLOSURE) ? 0 : f->value.cl->nelems;
}


int lua_currentline (lua_State *L, lua_Function f) {
  return (f+1 < L->top && (f+1)->ttype == LUA_T_LINE) ? (f+1)->value.i : -1;
}


lua_Object lua_getlocal (lua_State *L, lua_Function f, int local_number,
                         const char **name) {
  /* check whether `f' is a Lua function */
  if (lua_tag(L, f) != LUA_T_PROTO)
    return LUA_NOOBJECT;
  else {
    TProtoFunc *fp = luaA_protovalue(f)->value.tf;
    *name = luaF_getlocalname(fp, local_number, lua_currentline(L, f));
    if (*name) {
      /* if "*name", there must be a LUA_T_LINE */
      /* therefore, f+2 points to function base */
      return put_luaObject(L, (f+2)+(local_number-1));
    }
    else
      return LUA_NOOBJECT;
  }
}


int lua_setlocal (lua_State *L, lua_Function f, int local_number) {
  /* check whether `f' is a Lua function */
  if (lua_tag(L, f) != LUA_T_PROTO)
    return 0;
  else {
    TProtoFunc *fp = luaA_protovalue(f)->value.tf;
    const char *name = luaF_getlocalname(fp, local_number,
                                         lua_currentline(L, f));
    checkCparams(L, 1);
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
    if (normalized_type(f) == LUA_T_PROTO) {
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
  set_normalized(L->top, o); /* to be used by `checkfunc' */
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

/* }====================================================== */


/*
** {======================================================
** BLOCK mechanism
** =======================================================
*/


#ifndef	MAX_C_BLOCKS
#define MAX_C_BLOCKS	1000  /* arbitrary limit */
#endif


void lua_beginblock (lua_State *L) {
  luaM_growvector(L, L->Cblocks, L->numCblocks, 1, struct C_Lua_Stack,
                  "too many nested blocks", MAX_C_BLOCKS);
  L->Cblocks[L->numCblocks] = L->Cstack;
  L->numCblocks++;
}

void lua_endblock (lua_State *L) {
  --L->numCblocks;
  L->Cstack = L->Cblocks[L->numCblocks];
  L->top = L->Cstack.base;
}



int lua_ref (lua_State *L, int lock) {
  int ref;
  checkCparams(L, 1);
  ref = luaR_ref(L, L->top-1, lock);
  L->top--;
  return ref;
}



lua_Object lua_getref (lua_State *L, int ref) {
  const TObject *o = luaR_getref(L, ref);
  return (o ? put_luaObject(L, o) : LUA_NOOBJECT);
}

/* }====================================================== */

