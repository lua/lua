/*
** $Id: lapi.c,v 1.54 1999/10/14 19:13:31 roberto Exp roberto $
** Lua API
** See Copyright Notice in lua.h
*/


#include <string.h>

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



TObject *luaA_Address (lua_Object o) {
  return (o != LUA_NOOBJECT) ?  Address(o) : NULL;
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


static void checkCparams (int nParams) {
  if (L->stack.top-L->stack.stack < L->Cstack.base+nParams)
    lua_error("API error - wrong number of arguments in C2lua stack");
}


static lua_Object put_luaObject (const TObject *o) {
  luaD_openstack((L->stack.top-L->stack.stack)-L->Cstack.base);
  L->stack.stack[L->Cstack.base++] = *o;
  return L->Cstack.base;  /* this is +1 real position (see Ref) */
}


lua_Object luaA_putObjectOnTop (void) {
  luaD_openstack((L->stack.top-L->stack.stack)-L->Cstack.base);
  L->stack.stack[L->Cstack.base++] = *(--L->stack.top);
  return L->Cstack.base;  /* this is +1 real position (see Ref) */
}


static void top2LC (int n) {
  /* Put the 'n' elements on the top as the Lua2C contents */
  L->Cstack.base = (L->stack.top-L->stack.stack);  /* new base */
  L->Cstack.lua2C = L->Cstack.base-n;  /* position of the new results */
  L->Cstack.num = n;  /* number of results */
}


lua_Object lua_pop (void) {
  checkCparams(1);
  return luaA_putObjectOnTop();
}


/*
** Get a parameter, returning the object handle or LUA_NOOBJECT on error.
** 'number' must be 1 to get the first parameter.
*/
lua_Object lua_lua2C (int number) {
  if (number <= 0 || number > L->Cstack.num) return LUA_NOOBJECT;
  /* Ref(L->stack.stack+(L->Cstack.lua2C+number-1)) ==
     L->stack.stack+(L->Cstack.lua2C+number-1)-L->stack.stack+1 == */
  return L->Cstack.lua2C+number;
}


int lua_callfunction (lua_Object function) {
  if (function == LUA_NOOBJECT)
    return 1;
  else {
    luaD_openstack((L->stack.top-L->stack.stack)-L->Cstack.base);
    set_normalized(L->stack.stack+L->Cstack.base, Address(function));
    return luaD_protectedrun();
  }
}


lua_Object lua_gettagmethod (int tag, const char *event) {
  return put_luaObject(luaT_gettagmethod(tag, event));
}


lua_Object lua_settagmethod (int tag, const char *event) {
  checkCparams(1);
  luaT_settagmethod(tag, event, L->stack.top-1);
  return luaA_putObjectOnTop();
}


lua_Object lua_seterrormethod (void) {
  lua_Object temp;
  checkCparams(1);
  temp = lua_getglobal("_ERRORMESSAGE");
  lua_setglobal("_ERRORMESSAGE");
  return temp;
}


lua_Object lua_gettable (void) {
  checkCparams(2);
  luaV_gettable();
  return luaA_putObjectOnTop();
}


lua_Object lua_rawgettable (void) {
  checkCparams(2);
  if (ttype(L->stack.top-2) != LUA_T_ARRAY)
    lua_error("indexed expression not a table in rawgettable");
  *(L->stack.top-2) = *luaH_get(avalue(L->stack.top-2), L->stack.top-1);
  --L->stack.top;
  return luaA_putObjectOnTop();
}


void lua_settable (void) {
  checkCparams(3);
  luaV_settable(L->stack.top-3);
  L->stack.top -= 2;  /* pop table and index */
}


void lua_rawsettable (void) {
  checkCparams(3);
  luaV_rawsettable(L->stack.top-3);
}


lua_Object lua_createtable (void) {
  TObject o;
  luaC_checkGC();
  avalue(&o) = luaH_new(0);
  ttype(&o) = LUA_T_ARRAY;
  return put_luaObject(&o);
}


lua_Object lua_getglobal (const char *name) {
  luaD_checkstack(2);  /* may need that to call T.M. */
  luaV_getglobal(luaS_assertglobalbyname(name));
  return luaA_putObjectOnTop();
}


lua_Object lua_rawgetglobal (const char *name) {
  GlobalVar *gv = luaS_assertglobalbyname(name);
  return put_luaObject(&gv->value);
}


void lua_setglobal (const char *name) {
  checkCparams(1);
  luaD_checkstack(2);  /* may need that to call T.M. */
  luaV_setglobal(luaS_assertglobalbyname(name));
}


void lua_rawsetglobal (const char *name) {
  GlobalVar *gv = luaS_assertglobalbyname(name);
  checkCparams(1);
  gv->value = *(--L->stack.top);
}


const char *lua_type (lua_Object o) {
  return (o == LUA_NOOBJECT) ? "NOOBJECT" : luaO_typename(Address(o));
}

int lua_isnil (lua_Object o) {
  return (o!= LUA_NOOBJECT) && (ttype(Address(o)) == LUA_T_NIL);
}

int lua_istable (lua_Object o) {
  return (o!= LUA_NOOBJECT) && (ttype(Address(o)) == LUA_T_ARRAY);
}

int lua_isuserdata (lua_Object o) {
  return (o!= LUA_NOOBJECT) && (ttype(Address(o)) == LUA_T_USERDATA);
}

int lua_iscfunction (lua_Object o) {
  return (lua_tag(o) == LUA_T_CPROTO);
}

int lua_isnumber (lua_Object o) {
  return (o!= LUA_NOOBJECT) && (tonumber(Address(o)) == 0);
}

int lua_isstring (lua_Object o) {
  int t = lua_tag(o);
  return (t == LUA_T_STRING) || (t == LUA_T_NUMBER);
}

int lua_isfunction (lua_Object o) {
  int t = lua_tag(o);
  return (t == LUA_T_PROTO) || (t == LUA_T_CPROTO);
}


double lua_getnumber (lua_Object object) {
 if (object == LUA_NOOBJECT) return 0.0;
 if (tonumber(Address(object))) return 0.0;
 else return (nvalue(Address(object)));
}

const char *lua_getstring (lua_Object object) {
  luaC_checkGC();  /* "tostring" may create a new string */
  if (object == LUA_NOOBJECT || tostring(Address(object)))
    return NULL;
  else return (svalue(Address(object)));
}

long lua_strlen (lua_Object object) {
  luaC_checkGC();  /* "tostring" may create a new string */
  if (object == LUA_NOOBJECT || tostring(Address(object)))
    return 0L;
  else return (tsvalue(Address(object))->u.s.len);
}

void *lua_getuserdata (lua_Object object) {
  if (object == LUA_NOOBJECT || ttype(Address(object)) != LUA_T_USERDATA)
    return NULL;
  else return tsvalue(Address(object))->u.d.value;
}

lua_CFunction lua_getcfunction (lua_Object object) {
  if (!lua_iscfunction(object))
    return NULL;
  else return fvalue(luaA_protovalue(Address(object)));
}


void lua_pushnil (void) {
  ttype(L->stack.top) = LUA_T_NIL;
  incr_top;
}

void lua_pushnumber (double n) {
  ttype(L->stack.top) = LUA_T_NUMBER;
  nvalue(L->stack.top) = n;
  incr_top;
}

void lua_pushlstring (const char *s, long len) {
  tsvalue(L->stack.top) = luaS_newlstr(s, len);
  ttype(L->stack.top) = LUA_T_STRING;
  incr_top;
  luaC_checkGC();
}

void lua_pushstring (const char *s) {
  if (s == NULL)
    lua_pushnil();
  else
    lua_pushlstring(s, strlen(s));
}

void lua_pushcclosure (lua_CFunction fn, int n) {
  if (fn == NULL)
    lua_error("API error - attempt to push a NULL Cfunction");
  checkCparams(n);
  ttype(L->stack.top) = LUA_T_CPROTO;
  fvalue(L->stack.top) = fn;
  incr_top;
  luaV_closure(n);
  luaC_checkGC();
}

void lua_pushusertag (void *u, int tag) {
  if (tag < 0 && tag != LUA_ANYTAG)
    luaT_realtag(tag);  /* error if tag is not valid */
  tsvalue(L->stack.top) = luaS_createudata(u, tag);
  ttype(L->stack.top) = LUA_T_USERDATA;
  incr_top;
  luaC_checkGC();
}

void luaA_pushobject (const TObject *o) {
  *L->stack.top = *o;
  incr_top;
}

void lua_pushobject (lua_Object o) {
  if (o == LUA_NOOBJECT)
    lua_error("API error - attempt to push a NOOBJECT");
  set_normalized(L->stack.top, Address(o));
  incr_top;
}


int lua_tag (lua_Object lo) {
  if (lo == LUA_NOOBJECT)
     return LUA_T_NIL;
  else {
    const TObject *o = Address(lo);
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
        LUA_INTERNALERROR("invalid type");
#endif
      default:
        return t;
    }
  }
}


void lua_settag (int tag) {
  checkCparams(1);
  luaT_realtag(tag);
  switch (ttype(L->stack.top-1)) {
    case LUA_T_ARRAY:
      (L->stack.top-1)->value.a->htag = tag;
      break;
    case LUA_T_USERDATA:
      (L->stack.top-1)->value.ts->u.d.tag = tag;
      break;
    default:
      luaL_verror("cannot change the tag of a %.20s",
                  luaO_typename(L->stack.top-1));
  }
  L->stack.top--;
}


GlobalVar *luaA_nextvar (TaggedString *ts) {
  GlobalVar *gv;
  if (ts == NULL)
    gv = L->rootglobal;  /* first variable */
  else {
    /* check whether name is in global var list */
    luaL_arg_check(ts->u.s.gv, 1, "variable name expected");
    gv = ts->u.s.gv->next;  /* get next */
  }
  while (gv && gv->value.ttype == LUA_T_NIL)  /* skip globals with nil */
    gv = gv->next;
  if (gv) {
    ttype(L->stack.top) = LUA_T_STRING; tsvalue(L->stack.top) = gv->name;
    incr_top;
    luaA_pushobject(&gv->value);
  }
  return gv;
}


const char *lua_nextvar (const char *varname) {
  TaggedString *ts = (varname == NULL) ? NULL : luaS_new(varname);
  GlobalVar *gv = luaA_nextvar(ts);
  if (gv) {
    top2LC(2);
    return gv->name->str;
  }
  else {
    top2LC(0);
    return NULL;
  }
}


int luaA_next (const Hash *t, int i) {
  int tsize = t->size;
  for (; i<tsize; i++) {
    Node *n = node(t, i);
    if (ttype(val(n)) != LUA_T_NIL) {
      luaA_pushobject(key(n));
      luaA_pushobject(val(n));
      return i+1;  /* index to be used next time */
    }
  }
  return 0;  /* no more elements */
}


int lua_next (lua_Object o, int i) {
  const TObject *t = Address(o);
  if (ttype(t) != LUA_T_ARRAY)
    lua_error("API error - object is not a table in `lua_next'"); 
  i = luaA_next(avalue(t), i);
  top2LC((i==0) ? 0 : 2);
  return i;
}



/*
** {======================================================
** To manipulate some state information
** =======================================================
*/

lua_State *lua_setstate (lua_State *st) {
  lua_State *old = lua_state;
  lua_state = st;
  return old;
}

lua_LHFunction lua_setlinehook (lua_LHFunction func) {
  lua_LHFunction old = L->linehook;
  L->linehook = func;
  return old;
}

lua_CHFunction lua_setcallhook (lua_CHFunction func) {
  lua_CHFunction old = L->callhook;
  L->callhook = func;
  return old;
}

int lua_setdebug (int debug) {
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


lua_Function lua_stackedfunction (int level) {
  StkId i;
  for (i = (L->stack.top-1)-L->stack.stack; i>=0; i--) {
    int t = L->stack.stack[i].ttype;
    if (t == LUA_T_CLMARK || t == LUA_T_PMARK || t == LUA_T_CMARK)
      if (level-- == 0)
        return Ref(L->stack.stack+i);
  }
  return LUA_NOOBJECT;
}


int lua_nups (lua_Function func) {
  const TObject *o = luaA_Address(func);
  return (!o || normalized_type(o) != LUA_T_CLOSURE) ? 0 : o->value.cl->nelems;
}


int lua_currentline (lua_Function func) {
  const TObject *f = Address(func);
  return (f+1 < L->stack.top && (f+1)->ttype == LUA_T_LINE) ?
             (f+1)->value.i : -1;
}


lua_Object lua_getlocal (lua_Function func, int local_number,
                         const char **name) {
  /* check whether func is a Lua function */
  if (lua_tag(func) != LUA_T_PROTO)
    return LUA_NOOBJECT;
  else {
    TObject *f = Address(func);
    TProtoFunc *fp = luaA_protovalue(f)->value.tf;
    *name = luaF_getlocalname(fp, local_number, lua_currentline(func));
    if (*name) {
      /* if "*name", there must be a LUA_T_LINE */
      /* therefore, f+2 points to function base */
      return put_luaObject((f+2)+(local_number-1));
    }
    else
      return LUA_NOOBJECT;
  }
}


int lua_setlocal (lua_Function func, int local_number) {
  /* check whether func is a Lua function */
  if (lua_tag(func) != LUA_T_PROTO)
    return 0;
  else {
    TObject *f = Address(func);
    TProtoFunc *fp = luaA_protovalue(f)->value.tf;
    const char *name = luaF_getlocalname(fp, local_number, lua_currentline(func));
    checkCparams(1);
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


void lua_funcinfo (lua_Object func, const char **source, int *linedefined) {
  if (!lua_isfunction(func))
    lua_error("API error - `funcinfo' called with a non-function value");
  else {
    const TObject *f = luaA_protovalue(Address(func));
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


static int checkfunc (TObject *o) {
  return luaO_equalObj(o, L->stack.top);
}


const char *lua_getobjname (lua_Object o, const char **name) {
  /* try to find a name for given function */
  GlobalVar *g;
  set_normalized(L->stack.top, Address(o)); /* to be accessed by "checkfunc" */
  for (g=L->rootglobal; g; g=g->next) {
    if (checkfunc(&g->value)) {
      *name = g->name->str;
      return "global";
    }
  }
  /* not found: try tag methods */
  if ((*name = luaT_travtagmethods(checkfunc)) != NULL)
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


void lua_beginblock (void) {
  luaM_growvector(L->Cblocks, L->numCblocks, 1, struct C_Lua_Stack,
                  "too many nested blocks", MAX_C_BLOCKS);
  L->Cblocks[L->numCblocks] = L->Cstack;
  L->numCblocks++;
}

void lua_endblock (void) {
  --L->numCblocks;
  L->Cstack = L->Cblocks[L->numCblocks];
  luaD_adjusttop(L->Cstack.base);
}



int lua_ref (int lock) {
  int ref;
  checkCparams(1);
  ref = luaR_ref(L->stack.top-1, lock);
  L->stack.top--;
  return ref;
}



lua_Object lua_getref (int ref) {
  const TObject *o = luaR_getref(ref);
  return (o ? put_luaObject(o) : LUA_NOOBJECT);
}

/* }====================================================== */

