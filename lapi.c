/*
** $Id: lapi.c,v 1.1 1997/09/16 19:25:59 roberto Exp roberto $
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


void luaA_packresults (void)
{
  luaV_pack(luaD_Cstack.lua2C, luaD_Cstack.num, luaD_stack.top);
  incr_top;
}


int luaA_passresults (void)
{
  luaD_checkstack(luaD_Cstack.num);
  memcpy(luaD_stack.top, luaD_Cstack.lua2C+luaD_stack.stack,
         luaD_Cstack.num*sizeof(TObject));
  luaD_stack.top += luaD_Cstack.num;
  return luaD_Cstack.num;
}


static void checkCparams (int nParams)
{
  if (luaD_stack.top-luaD_stack.stack < luaD_Cstack.base+nParams)
    lua_error("API error - wrong number of arguments in C2lua stack");
}


static lua_Object put_luaObject (TObject *o)
{
  luaD_openstack((luaD_stack.top-luaD_stack.stack)-luaD_Cstack.base);
  luaD_stack.stack[luaD_Cstack.base++] = *o;
  return luaD_Cstack.base;  /* this is +1 real position (see Ref) */
}


static lua_Object put_luaObjectonTop (void)
{
  luaD_openstack((luaD_stack.top-luaD_stack.stack)-luaD_Cstack.base);
  luaD_stack.stack[luaD_Cstack.base++] = *(--luaD_stack.top);
  return luaD_Cstack.base;  /* this is +1 real position (see Ref) */
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
  if (number <= 0 || number > luaD_Cstack.num) return LUA_NOOBJECT;
  /* Ref(luaD_stack.stack+(luaD_Cstack.lua2C+number-1)) ==
     luaD_stack.stack+(luaD_Cstack.lua2C+number-1)-luaD_stack.stack+1 == */
  return luaD_Cstack.lua2C+number;
}


int lua_callfunction (lua_Object function)
{
  if (function == LUA_NOOBJECT)
    return 1;
  else {
    luaD_openstack((luaD_stack.top-luaD_stack.stack)-luaD_Cstack.base);
    luaD_stack.stack[luaD_Cstack.base] = *Address(function);
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
  luaT_settagmethod(tag, event, luaD_stack.top-1);
  return put_luaObjectonTop();
}


lua_Object lua_seterrormethod (void)
{
  TObject temp = luaD_errorim;
  checkCparams(1);
  luaD_errorim = *(--luaD_stack.top);
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
  if (ttype(luaD_stack.top-2) != LUA_T_ARRAY)
    lua_error("indexed expression not a table in raw gettable");
  else {
    TObject *h = luaH_get(avalue(luaD_stack.top-2), luaD_stack.top-1);
    --luaD_stack.top;
    if (h != NULL)
      *(luaD_stack.top-1) = *h;
    else
      ttype(luaD_stack.top-1) = LUA_T_NIL;
  }
  return put_luaObjectonTop();
}


void lua_settable (void)
{
  checkCparams(3);
  luaV_settable(luaD_stack.top-3, 1);
}


void lua_rawsettable (void)
{
  checkCparams(3);
  luaV_settable(luaD_stack.top-3, 0);
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
  return put_luaObject(&ts->u.globalval);
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
  luaS_rawsetglobal(ts, --luaD_stack.top);
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
  int t = lua_tag(o);
  return (t == LUA_T_CMARK) || (t == LUA_T_CFUNCTION);
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
  return (t == LUA_T_FUNCTION) || (t == LUA_T_CFUNCTION) ||
         (t == LUA_T_MARK) || (t == LUA_T_CMARK);
}


real lua_getnumber (lua_Object object)
{
 if (object == LUA_NOOBJECT) return 0.0;
 if (tonumber (Address(object))) return 0.0;
 else return (nvalue(Address(object)));
}

char *lua_getstring (lua_Object object)
{
  if (object == LUA_NOOBJECT || tostring(Address(object)))
    return NULL;
  else return (svalue(Address(object)));
}

void *lua_getuserdata (lua_Object object)
{
  if (object == LUA_NOOBJECT || ttype(Address(object)) != LUA_T_USERDATA)
    return NULL;
  else return tsvalue(Address(object))->u.d.v;
}

lua_CFunction lua_getcfunction (lua_Object object)
{
 if (object == LUA_NOOBJECT || ((ttype(Address(object)) != LUA_T_CFUNCTION) &&
                                (ttype(Address(object)) != LUA_T_CMARK)))
   return NULL;
 else return (fvalue(Address(object)));
}


void lua_pushnil (void)
{
  ttype(luaD_stack.top) = LUA_T_NIL;
  incr_top;
}

void lua_pushnumber (real n)
{
  ttype(luaD_stack.top) = LUA_T_NUMBER;
  nvalue(luaD_stack.top) = n;
  incr_top;
}

void lua_pushstring (char *s)
{
  if (s == NULL)
    ttype(luaD_stack.top) = LUA_T_NIL;
  else {
    tsvalue(luaD_stack.top) = luaS_new(s);
    ttype(luaD_stack.top) = LUA_T_STRING;
  }
  incr_top;
  luaC_checkGC();
}

void lua_pushcfunction (lua_CFunction fn)
{
  if (fn == NULL)
    ttype(luaD_stack.top) = LUA_T_NIL;
  else {
    ttype(luaD_stack.top) = LUA_T_CFUNCTION;
    fvalue(luaD_stack.top) = fn;
  }
  incr_top;
}

void lua_pushusertag (void *u, int tag)
{
  if (tag < 0 && tag != LUA_ANYTAG)
    luaT_realtag(tag);  /* error if tag is not valid */
  tsvalue(luaD_stack.top) = luaS_createudata(u, tag);
  ttype(luaD_stack.top) = LUA_T_USERDATA;
  incr_top;
  luaC_checkGC();
}

void luaA_pushobject (TObject *o)
{
  *luaD_stack.top = *o;
  incr_top;
}

void lua_pushobject (lua_Object o)
{
  if (o == LUA_NOOBJECT)
    lua_error("API error - attempt to push a NOOBJECT");
  *luaD_stack.top = *Address(o);
  if (ttype(luaD_stack.top) == LUA_T_MARK)
    ttype(luaD_stack.top) = LUA_T_FUNCTION;
  else if (ttype(luaD_stack.top) == LUA_T_CMARK)
    ttype(luaD_stack.top) = LUA_T_CFUNCTION;
  incr_top;
}


int lua_tag (lua_Object lo)
{
  if (lo == LUA_NOOBJECT) return LUA_T_NIL;
  else {
    TObject *o = Address(lo);
    lua_Type t = ttype(o);
    if (t == LUA_T_USERDATA)
      return o->value.ts->u.d.tag;
    else if (t == LUA_T_ARRAY)
      return o->value.a->htag;
    else return t;
  }
}


void lua_settag (int tag)
{
  checkCparams(1);
  luaT_realtag(tag);
  switch (ttype(luaD_stack.top-1)) {
    case LUA_T_ARRAY:
      (luaD_stack.top-1)->value.a->htag = tag;
      break;
    case LUA_T_USERDATA:
      (luaD_stack.top-1)->value.ts->u.d.tag = tag;
      break;
    default:
      luaL_verror("cannot change the tag of a %s",
                  luaO_typenames[-ttype((luaD_stack.top-1))]);
  }
  luaD_stack.top--;
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
  for (i = (luaD_stack.top-1)-luaD_stack.stack; i>=0; i--)
    if (luaD_stack.stack[i].ttype == LUA_T_MARK || luaD_stack.stack[i].ttype == LUA_T_CMARK)
      if (level-- == 0)
        return Ref(luaD_stack.stack+i);
  return LUA_NOOBJECT;
}


int lua_currentline (lua_Function func)
{
  TObject *f = Address(func);
  return (f+1 < luaD_stack.top && (f+1)->ttype == LUA_T_LINE) ? (f+1)->value.i : -1;
}


lua_Object lua_getlocal (lua_Function func, int local_number, char **name)
{
  TObject *f = luaA_Address(func);
  /* check whether func is a Lua function */
  if (ttype(f) != LUA_T_MARK && ttype(f) != LUA_T_FUNCTION)
    return LUA_NOOBJECT;
  *name = luaF_getlocalname(f->value.tf, local_number, lua_currentline(func));
  if (*name) {
    /* if "*name", there must be a LUA_T_LINE */
    /* therefore, f+2 points to function base */
    return Ref((f+2)+(local_number-1));
  }
  else
    return LUA_NOOBJECT;
}


int lua_setlocal (lua_Function func, int local_number)
{
  TObject *f = Address(func);
  char *name = luaF_getlocalname(f->value.tf, local_number,
                                 lua_currentline(func));
  checkCparams(1);
  --luaD_stack.top;
  if (name) {
    /* if "name", there must be a LUA_T_LINE */
    /* therefore, f+2 points to function base */
    *((f+2)+(local_number-1)) = *luaD_stack.top;
    return 1;
  }
  else
    return 0;
}


void lua_funcinfo (lua_Object func, char **filename, int *linedefined)
{
  TObject *f = Address(func);
  if (f->ttype == LUA_T_MARK || f->ttype == LUA_T_FUNCTION)
  {
    TProtoFunc *fp = f->value.cl->consts[0].value.tf;
    *filename = fp->fileName->str;
    *linedefined = fp->lineDefined;
  }
  else if (f->ttype == LUA_T_CMARK || f->ttype == LUA_T_CFUNCTION)
  {
    *filename = "(C)";
    *linedefined = -1;
  }
}


static TObject *functofind;
static int checkfunc (TObject *o)
{
  if (o->ttype == LUA_T_FUNCTION)
    return
       ((functofind->ttype == LUA_T_FUNCTION ||
         functofind->ttype == LUA_T_MARK) &&
        (functofind->value.cl == o->value.cl));
  else if (o->ttype == LUA_T_CFUNCTION)
    return
       ((functofind->ttype == LUA_T_CFUNCTION ||
         functofind->ttype == LUA_T_CMARK) &&
         (functofind->value.f == o->value.f));
  else return 0;
}


char *lua_getobjname (lua_Object o, char **name)
{ /* try to find a name for given function */
  functofind = Address(o);
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

#define MAX_C_BLOCKS 10

static int numCblocks = 0;
static struct C_Lua_Stack Cblocks[MAX_C_BLOCKS];

void lua_beginblock (void)
{
  if (numCblocks >= MAX_C_BLOCKS)
    lua_error("`lua_beginblock': too many nested blocks");
  Cblocks[numCblocks] = luaD_Cstack;
  numCblocks++;
}

void lua_endblock (void)
{
  --numCblocks;
  luaD_Cstack = Cblocks[numCblocks];
  luaD_adjusttop(luaD_Cstack.base);
}





int lua_ref (int lock)
{
  int ref;
  checkCparams(1);
  ref = luaC_ref(luaD_stack.top-1, lock);
  luaD_stack.top--;
  return ref;
}



lua_Object lua_getref (int ref)
{
  TObject *o = luaC_getref(ref);
  return (o ? put_luaObject(o) : LUA_NOOBJECT);
}





#if LUA_COMPAT2_5
/*
** API: set a function as a fallback
*/

static void do_unprotectedrun (lua_CFunction f, int nParams, int nResults)
{
  StkId base = (luaD_stack.top-luaD_stack.stack)-nParams;
  luaD_openstack(nParams);
  luaD_stack.stack[base].ttype = LUA_T_CFUNCTION;
  luaD_stack.stack[base].value.f = f;
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

