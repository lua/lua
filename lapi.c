/*
** $Id: lapi.c,v 1.57 1999/11/22 13:12:07 roberto Exp roberto $
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



TObject *luaA_Address (lua_State *L, lua_Object o) {
  return (o != LUA_NOOBJECT) ?  Address(L, o) : NULL;
}


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
  if (L->stack.top-L->stack.stack < L->Cstack.base+nParams)
    lua_error(L, "API error - wrong number of arguments in C2lua stack");
}


static lua_Object put_luaObject (lua_State *L, const TObject *o) {
  luaD_openstack(L, (L->stack.top-L->stack.stack)-L->Cstack.base);
  L->stack.stack[L->Cstack.base++] = *o;
  return L->Cstack.base;  /* this is +1 real position (see Ref) */
}


lua_Object luaA_putObjectOnTop (lua_State *L) {
  luaD_openstack(L, (L->stack.top-L->stack.stack)-L->Cstack.base);
  L->stack.stack[L->Cstack.base++] = *(--L->stack.top);
  return L->Cstack.base;  /* this is +1 real position (see Ref) */
}


static void top2LC (lua_State *L, int n) {
  /* Put the 'n' elements on the top as the Lua2C contents */
  L->Cstack.base = (L->stack.top-L->stack.stack);  /* new base */
  L->Cstack.lua2C = L->Cstack.base-n;  /* position of the new results */
  L->Cstack.num = n;  /* number of results */
}


lua_Object lua_pop (lua_State *L) {
  checkCparams(L, 1);
  return luaA_putObjectOnTop(L);
}


/*
** Get a parameter, returning the object handle or LUA_NOOBJECT on error.
** 'number' must be 1 to get the first parameter.
*/
lua_Object lua_lua2C (lua_State *L, int number) {
  if (number <= 0 || number > L->Cstack.num) return LUA_NOOBJECT;
  /* Ref(L, L->stack.stack+(L->Cstack.lua2C+number-1)) ==
     L->stack.stack+(L->Cstack.lua2C+number-1)-L->stack.stack+1 == */
  return L->Cstack.lua2C+number;
}


int lua_callfunction (lua_State *L, lua_Object function) {
  if (function == LUA_NOOBJECT)
    return 1;
  else {
    luaD_openstack(L, (L->stack.top-L->stack.stack)-L->Cstack.base);
    set_normalized(L->stack.stack+L->Cstack.base, Address(L, function));
    return luaD_protectedrun(L);
  }
}


lua_Object lua_gettagmethod (lua_State *L, int tag, const char *event) {
  return put_luaObject(L, luaT_gettagmethod(L, tag, event));
}


lua_Object lua_settagmethod (lua_State *L, int tag, const char *event) {
  checkCparams(L, 1);
  luaT_settagmethod(L, tag, event, L->stack.top-1);
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
  if (ttype(L->stack.top-2) != LUA_T_ARRAY)
    lua_error(L, "indexed expression not a table in rawgettable");
  *(L->stack.top-2) = *luaH_get(L, avalue(L->stack.top-2), L->stack.top-1);
  --L->stack.top;
  return luaA_putObjectOnTop(L);
}


void lua_settable (lua_State *L) {
  checkCparams(L, 3);
  luaV_settable(L, L->stack.top-3);
  L->stack.top -= 2;  /* pop table and index */
}


void lua_rawsettable (lua_State *L) {
  checkCparams(L, 3);
  luaV_rawsettable(L, L->stack.top-3);
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
  gv->value = *(--L->stack.top);
}


const char *lua_type (lua_State *L, lua_Object o) {
  return (o == LUA_NOOBJECT) ? "NOOBJECT" : luaO_typename(L, Address(L, o));
}

int lua_isnil (lua_State *L, lua_Object o) {
  return (o != LUA_NOOBJECT) && (ttype(Address(L, o)) == LUA_T_NIL);
}

int lua_istable (lua_State *L, lua_Object o) {
  return (o != LUA_NOOBJECT) && (ttype(Address(L, o)) == LUA_T_ARRAY);
}

int lua_isuserdata (lua_State *L, lua_Object o) {
  return (o != LUA_NOOBJECT) && (ttype(Address(L, o)) == LUA_T_USERDATA);
}

int lua_iscfunction (lua_State *L, lua_Object o) {
  return (lua_tag(L, o) == LUA_T_CPROTO);
}

int lua_isnumber (lua_State *L, lua_Object o) {
  return (o != LUA_NOOBJECT) && (tonumber(Address(L, o)) == 0);
}

int lua_isstring (lua_State *L, lua_Object o) {
  int t = lua_tag(L, o);
  return (t == LUA_T_STRING) || (t == LUA_T_NUMBER);
}

int lua_isfunction (lua_State *L, lua_Object o) {
  int t = lua_tag(L, o);
  return (t == LUA_T_PROTO) || (t == LUA_T_CPROTO);
}

int lua_equalobj (lua_State *L, lua_Object o1, lua_Object o2) {
  if (o1 == LUA_NOOBJECT || o2 == LUA_NOOBJECT) return 0;
  else return luaO_equalObj(Address(L, o1), Address(L, o2));
}


double lua_getnumber (lua_State *L, lua_Object object) {
 if (object == LUA_NOOBJECT) return 0.0;
 if (tonumber(Address(L, object))) return 0.0;
 else return (nvalue(Address(L, object)));
}

const char *lua_getstring (lua_State *L, lua_Object object) {
  luaC_checkGC(L);  /* `tostring' may create a new string */
  if (object == LUA_NOOBJECT || tostring(L, Address(L, object)))
    return NULL;
  else return (svalue(Address(L, object)));
}

long lua_strlen (lua_State *L, lua_Object object) {
  luaC_checkGC(L);  /* `tostring' may create a new string */
  if (object == LUA_NOOBJECT || tostring(L, Address(L, object)))
    return 0L;
  else return (tsvalue(Address(L, object))->u.s.len);
}

void *lua_getuserdata (lua_State *L, lua_Object object) {
  if (object == LUA_NOOBJECT || ttype(Address(L, object)) != LUA_T_USERDATA)
    return NULL;
  else return tsvalue(Address(L, object))->u.d.value;
}

lua_CFunction lua_getcfunction (lua_State *L, lua_Object object) {
  if (!lua_iscfunction(L, object))
    return NULL;
  else return fvalue(luaA_protovalue(Address(L, object)));
}


void lua_pushnil (lua_State *L) {
  ttype(L->stack.top) = LUA_T_NIL;
  incr_top;
}

void lua_pushnumber (lua_State *L, double n) {
  ttype(L->stack.top) = LUA_T_NUMBER;
  nvalue(L->stack.top) = n;
  incr_top;
}

void lua_pushlstring (lua_State *L, const char *s, long len) {
  tsvalue(L->stack.top) = luaS_newlstr(L, s, len);
  ttype(L->stack.top) = LUA_T_STRING;
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
  ttype(L->stack.top) = LUA_T_CPROTO;
  fvalue(L->stack.top) = fn;
  incr_top;
  luaV_closure(L, n);
  luaC_checkGC(L);
}

void lua_pushusertag (lua_State *L, void *u, int tag) {
  if (tag < 0 && tag != LUA_ANYTAG)
    luaT_realtag(L, tag);  /* error if tag is not valid */
  tsvalue(L->stack.top) = luaS_createudata(L, u, tag);
  ttype(L->stack.top) = LUA_T_USERDATA;
  incr_top;
  luaC_checkGC(L);
}

void luaA_pushobject (lua_State *L, const TObject *o) {
  *L->stack.top = *o;
  incr_top;
}

void lua_pushobject (lua_State *L, lua_Object o) {
  if (o == LUA_NOOBJECT)
    lua_error(L, "API error - attempt to push a NOOBJECT");
  set_normalized(L->stack.top, Address(L, o));
  incr_top;
}


int lua_tag (lua_State *L, lua_Object lo) {
  if (lo == LUA_NOOBJECT)
     return LUA_T_NIL;
  else {
    const TObject *o = Address(L, lo);
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
  switch (ttype(L->stack.top-1)) {
    case LUA_T_ARRAY:
      (L->stack.top-1)->value.a->htag = tag;
      break;
    case LUA_T_USERDATA:
      (L->stack.top-1)->value.ts->u.d.tag = tag;
      break;
    default:
      luaL_verror(L, "cannot change the tag of a %.20s",
                  luaO_typename(L, L->stack.top-1));
  }
  L->stack.top--;
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
    ttype(L->stack.top) = LUA_T_STRING; tsvalue(L->stack.top) = gv->name;
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


int lua_next (lua_State *L, lua_Object o, int i) {
  const TObject *t = Address(L, o);
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
  StkId i;
  for (i = (L->stack.top-1)-L->stack.stack; i>=0; i--) {
    int t = L->stack.stack[i].ttype;
    if (t == LUA_T_CLMARK || t == LUA_T_PMARK || t == LUA_T_CMARK)
      if (level-- == 0)
        return Ref(L, L->stack.stack+i);
  }
  return LUA_NOOBJECT;
}


int lua_nups (lua_State *L, lua_Function func) {
  const TObject *o = luaA_Address(L, func);
  return (!o || normalized_type(o) != LUA_T_CLOSURE) ? 0 : o->value.cl->nelems;
}


int lua_currentline (lua_State *L, lua_Function func) {
  const TObject *f = Address(L, func);
  return (f+1 < L->stack.top && (f+1)->ttype == LUA_T_LINE) ?
             (f+1)->value.i : -1;
}


lua_Object lua_getlocal (lua_State *L, lua_Function func, int local_number,
                         const char **name) {
  /* check whether func is a Lua function */
  if (lua_tag(L, func) != LUA_T_PROTO)
    return LUA_NOOBJECT;
  else {
    TObject *f = Address(L, func);
    TProtoFunc *fp = luaA_protovalue(f)->value.tf;
    *name = luaF_getlocalname(fp, local_number, lua_currentline(L, func));
    if (*name) {
      /* if "*name", there must be a LUA_T_LINE */
      /* therefore, f+2 points to function base */
      return put_luaObject(L, (f+2)+(local_number-1));
    }
    else
      return LUA_NOOBJECT;
  }
}


int lua_setlocal (lua_State *L, lua_Function func, int local_number) {
  /* check whether func is a Lua function */
  if (lua_tag(L, func) != LUA_T_PROTO)
    return 0;
  else {
    TObject *f = Address(L, func);
    TProtoFunc *fp = luaA_protovalue(f)->value.tf;
    const char *name = luaF_getlocalname(fp, local_number,
                                         lua_currentline(L, func));
    checkCparams(L, 1);
    --L->stack.top;
    if (name) {
      /* if "name", there must be a LUA_T_LINE */
      /* therefore, f+2 points to function base */
      *((f+2)+(local_number-1)) = *L->stack.top;
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
    const TObject *f = luaA_protovalue(Address(L, func));
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
  return luaO_equalObj(o, L->stack.top);
}


const char *lua_getobjname (lua_State *L, lua_Object o, const char **name) {
  /* try to find a name for given function */
  GlobalVar *g;
  set_normalized(L->stack.top, Address(L, o)); /* to be used by `checkfunc' */
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
  luaD_adjusttop(L, L->Cstack.base);
}



int lua_ref (lua_State *L, int lock) {
  int ref;
  checkCparams(L, 1);
  ref = luaR_ref(L, L->stack.top-1, lock);
  L->stack.top--;
  return ref;
}



lua_Object lua_getref (lua_State *L, int ref) {
  const TObject *o = luaR_getref(L, ref);
  return (o ? put_luaObject(L, o) : LUA_NOOBJECT);
}

/* }====================================================== */

