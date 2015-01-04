/*
** $Id: lapi.c,v 1.25 1998/06/05 22:17:44 roberto Exp $
** Lua API
** See Copyright Notice in lua.h
*/


#include <stdlib.h>
#include <string.h>

#include "lapi.h"
#include "lauxlib.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lua.h"
#include "luadebug.h"
#include "lvm.h"


char lua_ident[] = "$Lua: " LUA_VERSION " " LUA_COPYRIGHT " $\n"
                   "$Autores:  " LUA_AUTHORS " $";



TObject *luaA_Address (lua_Object o)
{
  return Address(o);
}


static int normalized_type (TObject *o)
{
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


static void set_normalized (TObject *d, TObject *s)
{
  d->value = s->value;
  d->ttype = normalized_type(s);
}


static TObject *luaA_protovalue (TObject *o)
{
  return (normalized_type(o) == LUA_T_CLOSURE) ?  protovalue(o) : o;
}


void luaA_packresults (void)
{
  luaV_pack(L->Cstack.lua2C, L->Cstack.num, L->stack.top);
  incr_top;
}


int luaA_passresults (void)
{
  luaD_checkstack(L->Cstack.num);
  memcpy(L->stack.top, L->Cstack.lua2C+L->stack.stack,
         L->Cstack.num*sizeof(TObject));
  L->stack.top += L->Cstack.num;
  return L->Cstack.num;
}


static void checkCparams (int nParams)
{
  if (L->stack.top-L->stack.stack < L->Cstack.base+nParams)
    lua_error("API error - wrong number of arguments in C2lua stack");
}


static lua_Object put_luaObject (TObject *o)
{
  luaD_openstack((L->stack.top-L->stack.stack)-L->Cstack.base);
  L->stack.stack[L->Cstack.base++] = *o;
  return L->Cstack.base;  /* this is +1 real position (see Ref) */
}


static lua_Object put_luaObjectonTop (void)
{
  luaD_openstack((L->stack.top-L->stack.stack)-L->Cstack.base);
  L->stack.stack[L->Cstack.base++] = *(--L->stack.top);
  return L->Cstack.base;  /* this is +1 real position (see Ref) */
}


lua_Object lua_pop (void)
{
  checkCparams(1);
  return put_luaObjectonTop();
}


/*
** Get a parameter, returning the object handle or LUA_NOOBJECT on error.
** 'number' must be 1 to get the first parameter.
*/
lua_Object lua_lua2C (int number)
{
  if (number <= 0 || number > L->Cstack.num) return LUA_NOOBJECT;
  /* Ref(L->stack.stack+(L->Cstack.lua2C+number-1)) ==
     L->stack.stack+(L->Cstack.lua2C+number-1)-L->stack.stack+1 == */
  return L->Cstack.lua2C+number;
}


int lua_callfunction (lua_Object function)
{
  if (function == LUA_NOOBJECT)
    return 1;
  else {
    luaD_openstack((L->stack.top-L->stack.stack)-L->Cstack.base);
    set_normalized(L->stack.stack+L->Cstack.base, Address(function));
    return luaD_protectedrun(MULT_RET);
  }
}


lua_Object lua_gettagmethod (int tag, char *event)
{
  return put_luaObject(luaT_gettagmethod(tag, event));
}


lua_Object lua_settagmethod (int tag, char *event)
{
  checkCparams(1);
  luaT_settagmethod(tag, event, L->stack.top-1);
  return put_luaObjectonTop();
}


lua_Object lua_seterrormethod (void)
{
  TObject temp = L->errorim;
  checkCparams(1);
  L->errorim = *(--L->stack.top);
  return put_luaObject(&temp);
}


lua_Object lua_gettable (void)
{
  checkCparams(2);
  luaV_gettable();
  return put_luaObjectonTop();
}


lua_Object lua_rawgettable (void)
{
  checkCparams(2);
  if (ttype(L->stack.top-2) != LUA_T_ARRAY)
    lua_error("indexed expression not a table in rawgettable");
  else {
    TObject *h = luaH_get(avalue(L->stack.top-2), L->stack.top-1);
    --L->stack.top;
    if (h != NULL)
      *(L->stack.top-1) = *h;
    else
      ttype(L->stack.top-1) = LUA_T_NIL;
  }
  return put_luaObjectonTop();
}


void lua_settable (void)
{
  checkCparams(3);
  luaV_settable(L->stack.top-3, 1);
}


void lua_rawsettable (void)
{
  checkCparams(3);
  luaV_settable(L->stack.top-3, 0);
}


lua_Object lua_createtable (void)
{
  TObject o;
  luaC_checkGC();
  avalue(&o) = luaH_new(0);
  ttype(&o) = LUA_T_ARRAY;
  return put_luaObject(&o);
}


lua_Object lua_getglobal (char *name)
{
  luaD_checkstack(2);  /* may need that to call T.M. */
  luaV_getglobal(luaS_new(name));
  return put_luaObjectonTop();
}


lua_Object lua_rawgetglobal (char *name)
{
  TaggedString *ts = luaS_new(name);
  return put_luaObject(&ts->u.s.globalval);
}


void lua_setglobal (char *name)
{
  checkCparams(1);
  luaD_checkstack(2);  /* may need that to call T.M. */
  luaV_setglobal(luaS_new(name));
}


void lua_rawsetglobal (char *name)
{
  TaggedString *ts = luaS_new(name);
  checkCparams(1);
  luaS_rawsetglobal(ts, --L->stack.top);
}



int lua_isnil (lua_Object o)
{
  return (o!= LUA_NOOBJECT) && (ttype(Address(o)) == LUA_T_NIL);
}

int lua_istable (lua_Object o)
{
  return (o!= LUA_NOOBJECT) && (ttype(Address(o)) == LUA_T_ARRAY);
}

int lua_isuserdata (lua_Object o)
{
  return (o!= LUA_NOOBJECT) && (ttype(Address(o)) == LUA_T_USERDATA);
}

int lua_iscfunction (lua_Object o)
{
  return (lua_tag(o) == LUA_T_CPROTO);
}

int lua_isnumber (lua_Object o)
{
  return (o!= LUA_NOOBJECT) && (tonumber(Address(o)) == 0);
}

int lua_isstring (lua_Object o)
{
  int t = lua_tag(o);
  return (t == LUA_T_STRING) || (t == LUA_T_NUMBER);
}

int lua_isfunction (lua_Object o)
{
  int t = lua_tag(o);
  return (t == LUA_T_PROTO) || (t == LUA_T_CPROTO);
}


double lua_getnumber (lua_Object object)
{
 if (object == LUA_NOOBJECT) return 0.0;
 if (tonumber(Address(object))) return 0.0;
 else return (nvalue(Address(object)));
}

char *lua_getstring (lua_Object object)
{
  luaC_checkGC();  /* "tostring" may create a new string */
  if (object == LUA_NOOBJECT || tostring(Address(object)))
    return NULL;
  else return (svalue(Address(object)));
}

long lua_strlen (lua_Object object)
{
  luaC_checkGC();  /* "tostring" may create a new string */
  if (object == LUA_NOOBJECT || tostring(Address(object)))
    return 0L;
  else return (tsvalue(Address(object))->u.s.len);
}

void *lua_getuserdata (lua_Object object)
{
  if (object == LUA_NOOBJECT || ttype(Address(object)) != LUA_T_USERDATA)
    return NULL;
  else return tsvalue(Address(object))->u.d.v;
}

lua_CFunction lua_getcfunction (lua_Object object)
{
  if (!lua_iscfunction(object))
    return NULL;
  else return fvalue(luaA_protovalue(Address(object)));
}


void lua_pushnil (void)
{
  ttype(L->stack.top) = LUA_T_NIL;
  incr_top;
}

void lua_pushnumber (double n)
{
  ttype(L->stack.top) = LUA_T_NUMBER;
  nvalue(L->stack.top) = n;
  incr_top;
}

void lua_pushlstring (char *s, long len)
{
  tsvalue(L->stack.top) = luaS_newlstr(s, len);
  ttype(L->stack.top) = LUA_T_STRING;
  incr_top;
  luaC_checkGC();
}

void lua_pushstring (char *s)
{
  if (s == NULL)
    lua_pushnil();
  else
    lua_pushlstring(s, strlen(s));
}

void lua_pushcclosure (lua_CFunction fn, int n)
{
  if (fn == NULL)
    lua_error("API error - attempt to push a NULL Cfunction");
  checkCparams(n);
  ttype(L->stack.top) = LUA_T_CPROTO;
  fvalue(L->stack.top) = fn;
  incr_top;
  luaV_closure(n);
  luaC_checkGC();
}

void lua_pushusertag (void *u, int tag)
{
  if (tag < 0 && tag != LUA_ANYTAG)
    luaT_realtag(tag);  /* error if tag is not valid */
  tsvalue(L->stack.top) = luaS_createudata(u, tag);
  ttype(L->stack.top) = LUA_T_USERDATA;
  incr_top;
  luaC_checkGC();
}

void luaA_pushobject (TObject *o)
{
  *L->stack.top = *o;
  incr_top;
}

void lua_pushobject (lua_Object o)
{
  if (o == LUA_NOOBJECT)
    lua_error("API error - attempt to push a NOOBJECT");
  else {
    set_normalized(L->stack.top, Address(o));
    incr_top;
  }
}


int lua_tag (lua_Object lo)
{
  if (lo == LUA_NOOBJECT)
     return LUA_T_NIL;
  else {
    TObject *o = Address(lo);
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


void lua_settag (int tag)
{
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
                  luaO_typenames[-ttype((L->stack.top-1))]);
  }
  L->stack.top--;
}


/*
** =======================================================
** Debug interface
** =======================================================
*/


/* Hooks */
lua_CHFunction lua_callhook = NULL;
lua_LHFunction lua_linehook = NULL;


lua_Function lua_stackedfunction (int level)
{
  StkId i;
  for (i = (L->stack.top-1)-L->stack.stack; i>=0; i--) {
    int t = L->stack.stack[i].ttype;
    if (t == LUA_T_CLMARK || t == LUA_T_PMARK || t == LUA_T_CMARK)
      if (level-- == 0)
        return Ref(L->stack.stack+i);
  }
  return LUA_NOOBJECT;
}


int lua_currentline (lua_Function func)
{
  TObject *f = Address(func);
  return (f+1 < L->stack.top && (f+1)->ttype == LUA_T_LINE) ?
             (f+1)->value.i : -1;
}


lua_Object lua_getlocal (lua_Function func, int local_number, char **name)
{
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
      return Ref((f+2)+(local_number-1));
    }
    else
      return LUA_NOOBJECT;
  }
}


int lua_setlocal (lua_Function func, int local_number)
{
  /* check whether func is a Lua function */
  if (lua_tag(func) != LUA_T_PROTO)
    return 0;
  else {
    TObject *f = Address(func);
    TProtoFunc *fp = luaA_protovalue(f)->value.tf;
    char *name = luaF_getlocalname(fp, local_number, lua_currentline(func));
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


void lua_funcinfo (lua_Object func, char **filename, int *linedefined)
{
  if (!lua_isfunction(func))
    lua_error("API - `funcinfo' called with a non-function value");
  else {
    TObject *f = luaA_protovalue(Address(func));
    if (normalized_type(f) == LUA_T_PROTO) {
      *filename = tfvalue(f)->fileName->str;
      *linedefined = tfvalue(f)->lineDefined;
    }
    else {
      *filename = "(C)";
      *linedefined = -1;
    }
  }
}


static int checkfunc (TObject *o)
{
  return luaO_equalObj(o, L->stack.top);
}


char *lua_getobjname (lua_Object o, char **name)
{ /* try to find a name for given function */
  set_normalized(L->stack.top, Address(o)); /* to be accessed by "checkfunc */
  if ((*name = luaT_travtagmethods(checkfunc)) != NULL)
    return "tag-method";
  else if ((*name = luaS_travsymbol(checkfunc)) != NULL)
    return "global";
  else return "";
}

/*
** =======================================================
** BLOCK mechanism
** =======================================================
*/


void lua_beginblock (void)
{
  if (L->numCblocks >= MAX_C_BLOCKS)
    lua_error("too many nested blocks");
  L->Cblocks[L->numCblocks] = L->Cstack;
  L->numCblocks++;
}

void lua_endblock (void)
{
  --L->numCblocks;
  L->Cstack = L->Cblocks[L->numCblocks];
  luaD_adjusttop(L->Cstack.base);
}



int lua_ref (int lock)
{
  int ref;
  checkCparams(1);
  ref = luaC_ref(L->stack.top-1, lock);
  L->stack.top--;
  return ref;
}



lua_Object lua_getref (int ref)
{
  TObject *o = luaC_getref(ref);
  return (o ? put_luaObject(o) : LUA_NOOBJECT);
}


/*
** =======================================================
** Derived functions
** =======================================================
*/
int (lua_call) (char *name) { return lua_call(name); }

void (lua_pushref) (int ref) { lua_pushref(ref); }

int (lua_refobject) (lua_Object o, int l) { return lua_refobject(o, l); }

void (lua_register) (char *n, lua_CFunction f) { lua_register(n, f); }

void (lua_pushuserdata) (void *u) { lua_pushuserdata(u); }

void (lua_pushcfunction) (lua_CFunction f) { lua_pushcfunction(f); }

int (lua_clonetag) (int t) { return lua_clonetag(t); }




#ifdef LUA_COMPAT2_5
/*
** API: set a function as a fallback
*/

static void do_unprotectedrun (lua_CFunction f, int nParams, int nResults)
{
  StkId base = (L->stack.top-L->stack.stack)-nParams;
  luaD_openstack(nParams);
  L->stack.stack[base].ttype = LUA_T_CPROTO;
  L->stack.stack[base].value.f = f;
  luaD_call(base+1, nResults);
}

lua_Object lua_setfallback (char *name, lua_CFunction fallback)
{
  lua_pushstring(name);
  lua_pushcfunction(fallback);
  do_unprotectedrun(luaT_setfallback, 2, 1);
  return put_luaObjectonTop();
}
#endif

