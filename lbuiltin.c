/*
** $Id: lbuiltin.c,v 1.34 1998/08/21 17:43:44 roberto Exp roberto $
** Built-in functions
** See Copyright Notice in lua.h
*/


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lapi.h"
#include "lauxlib.h"
#include "lbuiltin.h"
#include "ldo.h"
#include "lfunc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lua.h"
#include "lundump.h"
#include "lvm.h"



static void pushstring (TaggedString *s)
{
  TObject o;
  o.ttype = LUA_T_STRING;
  o.value.ts = s;
  luaA_pushobject(&o);
}


static int getsize (TObject *t) {
  int max = 0;
  int i;
  Hash *h = avalue(t);
  LUA_ASSERT(ttype(t) == LUA_T_ARRAY, "table expected");
  for (i = 0; i<nhash(h); i++) {
    Node *n = h->node+i;
    if (ttype(ref(n)) == LUA_T_NUMBER && ttype(val(n)) != LUA_T_NIL &&
                                         (int)nvalue(ref(n)) > max)
      max = nvalue(ref(n));
  }
  return max;
}


static int getnarg (lua_Object table) {
  lua_Object temp;
  /* temp = table.n */
  lua_pushobject(table); lua_pushstring("n"); temp = lua_rawgettable();
  return (lua_isnumber(temp) ? lua_getnumber(temp) :
                               getsize(luaA_Address(table)));
}


static void getn (void) {
  lua_pushnumber(getnarg(luaL_tablearg(1)));
}


static void nextvar (void) {
  TObject *o = luaA_Address(luaL_nonnullarg(1));
  TaggedString *g;
  if (ttype(o) == LUA_T_NIL)
    g = (TaggedString *)L->rootglobal.next;  /* first variable */
  else {
    luaL_arg_check(ttype(o) == LUA_T_STRING, 1, "variable name expected");
    g = tsvalue(o);  /* find given variable name */
    /* check whether name is in global var list */
    luaL_arg_check((GCnode *)g != g->head.next, 1, "variable name expected");
    g = (TaggedString *)g->head.next;  /* get next */
  }
  while (g && g->u.s.globalval.ttype == LUA_T_NIL)  /* skip globals with nil */
    g = (TaggedString *)g->head.next;
  if (g) {
    pushstring(g);
    luaA_pushobject(&g->u.s.globalval);
  }
  else lua_pushnil();  /* no more globals */
}


static void foreachvar (void) {
  TObject f = *luaA_Address(luaL_functionarg(1));
  GCnode *g;
  StkId name = L->Cstack.base++;  /* place to keep var name (to avoid GC) */
  luaD_checkstack(4);  /* for var name, f, s, and globalvar */
  ttype(L->stack.stack+name) = LUA_T_NIL;
  L->stack.top++;  /* top == base */
  for (g = L->rootglobal.next; g; g = g->next) {
    TaggedString *s = (TaggedString *)g;
    if (s->u.s.globalval.ttype != LUA_T_NIL) {
      ttype(L->stack.stack+name) = LUA_T_STRING;
      tsvalue(L->stack.stack+name) = s;  /* keep s on stack to avoid GC */
      *(L->stack.top++) = f;
      pushstring(s);
      *(L->stack.top++) = s->u.s.globalval;
      luaD_calln(2, 1);
      if (ttype(L->stack.top-1) != LUA_T_NIL)
        return;
      L->stack.top--;
    }
  }
}


static void next (void) {
  Node *n = luaH_next(luaA_Address(luaL_tablearg(1)),
                      luaA_Address(luaL_nonnullarg(2)));
  if (n) {
    luaA_pushobject(&n->ref);
    luaA_pushobject(&n->val);
  }
  else lua_pushnil();
}


static void foreach (void) {
  TObject t = *luaA_Address(luaL_tablearg(1));
  TObject f = *luaA_Address(luaL_functionarg(2));
  int i;
  luaD_checkstack(3);  /* for f, ref, and val */
  for (i=0; i<avalue(&t)->nhash; i++) {
    Node *nd = &(avalue(&t)->node[i]);
    if (ttype(ref(nd)) != LUA_T_NIL && ttype(val(nd)) != LUA_T_NIL) {
      *(L->stack.top++) = f;
      *(L->stack.top++) = *ref(nd);
      *(L->stack.top++) = *val(nd);
      luaD_calln(2, 1);
      if (ttype(L->stack.top-1) != LUA_T_NIL)
        return;
      L->stack.top--;
    }
  }
}


static void foreachi (void) {
  lua_Object ot = luaL_tablearg(1);
  Hash *t = avalue(luaA_Address(ot));
  TObject f = *luaA_Address(luaL_functionarg(2));
  int i;
  int n = getnarg(ot);
  luaD_checkstack(3);  /* for f, ref, and val */
  for (i=1; i<=n; i++) {
    *(L->stack.top++) = f;
    ttype(L->stack.top) = LUA_T_NUMBER;
    nvalue(L->stack.top++) = i;
    *(L->stack.top++) = *luaH_getint(t, i);
    luaD_calln(2, 1);
    if (ttype(L->stack.top-1) != LUA_T_NIL)
      return;
    L->stack.top--;
  }
}


static void internaldostring (void)
{
  long l;
  char *s = luaL_check_lstr(1, &l);
  if (*s == ID_CHUNK)
    lua_error("`dostring' cannot run pre-compiled code");
  if (lua_dobuffer(s, l, luaL_opt_string(2, NULL)) == 0)
    if (luaA_passresults() == 0)
      lua_pushuserdata(NULL);  /* at least one result to signal no errors */
}


static void internaldofile (void)
{
  char *fname = luaL_opt_string(1, NULL);
  if (lua_dofile(fname) == 0)
    if (luaA_passresults() == 0)
      lua_pushuserdata(NULL);  /* at least one result to signal no errors */
}


static void to_string (void) {
  lua_Object obj = lua_getparam(1);
  TObject *o = luaA_Address(obj);
  char buff[32];
  switch (ttype(o)) {
    case LUA_T_NUMBER:
      lua_pushstring(lua_getstring(obj));
      return;
    case LUA_T_STRING:
      lua_pushobject(obj);
      return;
    case LUA_T_ARRAY: {
      sprintf(buff, "table: %p", (void *)o->value.a);
      break;
    }
    case LUA_T_CLOSURE: {
      sprintf(buff, "function: %p", (void *)o->value.cl);
      break;
    }
    case LUA_T_PROTO: {
      sprintf(buff, "function: %p", (void *)o->value.tf);
      break;
    }
    case LUA_T_CPROTO: {
      sprintf(buff, "function: %p", (void *)o->value.f);
      break;
    }
    case LUA_T_USERDATA: {
      sprintf(buff, "userdata: %p", o->value.ts->u.d.v);
      break;
    }
    case LUA_T_NIL:
      lua_pushstring("nil");
      return;
    default:
      LUA_INTERNALERROR("invalid type");
  }
  lua_pushstring(buff);
}


static void luaI_print (void) {
  TaggedString *ts = luaS_new("tostring");
  lua_Object obj;
  int i = 1;
  while ((obj = lua_getparam(i++)) != LUA_NOOBJECT) {
    luaA_pushobject(&ts->u.s.globalval);
    lua_pushobject(obj);
    luaD_calln(1, 1);
    if (ttype(L->stack.top-1) != LUA_T_STRING)
      lua_error("`tostring' must return a string to `print'");
    printf("%.200s\t", svalue(L->stack.top-1));
    L->stack.top--;
  }
  printf("\n");
}


static void luaB_message (void) {
  fputs(luaL_check_string(1), stderr);
}


static void error_message (void) {
  char buff[200];
  sprintf(buff, "lua error: %.180s\n", luaL_check_string(1));
  lua_pushstring(buff);
  lua_call("_ALERT");
}


static void luaI_type (void)
{
  lua_Object o = luaL_nonnullarg(1);
  lua_pushstring(luaO_typename(luaA_Address(o)));
  lua_pushnumber(lua_tag(o));
}


static void luaB_tonumber (void)
{
  int base = luaL_opt_number(2, 10);
  if (base == 10) {  /* standard conversion */
    lua_Object o = lua_getparam(1);
    if (lua_isnumber(o))
      lua_pushnumber(lua_getnumber(o));
  }
  else {
    char *s = luaL_check_string(1);
    unsigned long n;
    luaL_arg_check(0 <= base && base <= 36, 2, "base out of range");
    n = strtol(s, &s, base);
    while (isspace(*s)) s++;  /* skip trailing spaces */
    if (*s) lua_pushnil();  /* invalid format: return nil */
    else lua_pushnumber(n);
  }
}


static void luaI_error (void)
{
  lua_error(lua_getstring(lua_getparam(1)));
}


static void luaI_assert (void)
{
  lua_Object p = lua_getparam(1);
  if (p == LUA_NOOBJECT || lua_isnil(p))
    luaL_verror("assertion failed!  %.100s", luaL_opt_string(2, ""));
}


static void setglobal (void)
{
  char *n = luaL_check_string(1);
  lua_Object value = luaL_nonnullarg(2);
  lua_pushobject(value);
  lua_setglobal(n);
  lua_pushobject(value);  /* return given value */
}

static void rawsetglobal (void)
{
  char *n = luaL_check_string(1);
  lua_Object value = luaL_nonnullarg(2);
  lua_pushobject(value);
  lua_rawsetglobal(n);
  lua_pushobject(value);  /* return given value */
}

static void getglobal (void)
{
  lua_pushobject(lua_getglobal(luaL_check_string(1)));
}

static void rawgetglobal (void)
{
  lua_pushobject(lua_rawgetglobal(luaL_check_string(1)));
}

static void luatag (void)
{
  lua_pushnumber(lua_tag(lua_getparam(1)));
}


static void luaI_call (void) {
  lua_Object f = luaL_nonnullarg(1);
  lua_Object arg = luaL_tablearg(2);
  char *options = luaL_opt_string(3, "");
  lua_Object err = lua_getparam(4);
  int narg = getnarg(arg);
  int i, status;
  if (err != LUA_NOOBJECT) {  /* set new error method */
    lua_pushobject(err);
    err = lua_seterrormethod();
  }
  /* push arg[1...n] */
  luaD_checkstack(narg);
  for (i=0; i<narg; i++)
    *(L->stack.top++) = *luaH_getint(avalue(luaA_Address(arg)), i+1);
  status = lua_callfunction(f);
  if (err != LUA_NOOBJECT) {  /* restore old error method */
    lua_pushobject(err);
    lua_seterrormethod();
  }
  if (status != 0) {  /* error in call? */
    if (strchr(options, 'x')) {
      lua_pushnil();
      return;  /* return nil to signal the error */
    }
    else
      lua_error(NULL);
  }
  else {  /* no errors */
    if (strchr(options, 'p'))
      luaA_packresults();
    else
      luaA_passresults();
  }
}


static void settag (void)
{
  lua_Object o = luaL_tablearg(1);
  lua_pushobject(o);
  lua_settag(luaL_check_number(2));
  lua_pushobject(o);  /* returns first argument */
}


static void newtag (void)
{
  lua_pushnumber(lua_newtag());
}


static void copytagmethods (void)
{
  lua_pushnumber(lua_copytagmethods(luaL_check_number(1),
                                    luaL_check_number(2)));
}


static void rawgettable (void)
{
  lua_pushobject(luaL_nonnullarg(1));
  lua_pushobject(luaL_nonnullarg(2));
  lua_pushobject(lua_rawgettable());
}


static void rawsettable (void)
{
  lua_pushobject(luaL_nonnullarg(1));
  lua_pushobject(luaL_nonnullarg(2));
  lua_pushobject(luaL_nonnullarg(3));
  lua_rawsettable();
}


static void settagmethod (void)
{
  lua_Object nf = luaL_nonnullarg(3);
  lua_pushobject(nf);
  lua_pushobject(lua_settagmethod((int)luaL_check_number(1),
                                  luaL_check_string(2)));
}


static void gettagmethod (void)
{
  lua_pushobject(lua_gettagmethod((int)luaL_check_number(1),
                                  luaL_check_string(2)));
}


static void seterrormethod (void)
{
  lua_Object nf = luaL_functionarg(1);
  lua_pushobject(nf);
  lua_pushobject(lua_seterrormethod());
}


static void luaI_collectgarbage (void)
{
  lua_pushnumber(lua_collectgarbage(luaL_opt_number(1, 0)));
}



static void swap (Hash *a, int i, int j) {
  /* notice: must use two temporary vars, because luaH_setint may cause a
     rehash and change the addresses of values in the array */
  TObject ai = *luaH_getint(a, i);
  TObject aj = *luaH_getint(a, j);
  luaH_setint(a, i, &aj);
  luaH_setint(a, j, &ai);
}

static int sort_comp (TObject *f, TObject *a, TObject *b) {
  /* notice: the caller (auxsort) must check stack space */
  if (f) {
    *(L->stack.top++) = *f;
    *(L->stack.top++) = *a;
    *(L->stack.top++) = *b;
    luaD_calln(2, 1);
  }
  else {  /* a < b? */
    *(L->stack.top++) = *a;
    *(L->stack.top++) = *b;
    luaV_comparison(LUA_T_NUMBER, LUA_T_NIL, LUA_T_NIL, IM_LT);
  }
  return ttype(--(L->stack.top)) != LUA_T_NIL;
}

/*
** quicksort algorithm from "Programming Pearls", pg. 112
*/
static void auxsort (Hash *a, int l, int u, TObject *f) {
  init:
  if (u <= l) return;  /* 0 or 1 element */
  luaD_checkstack(4);  /* for pivot, f, a, b (sort_comp) */
  if (u-l == 1) {  /* only two elements? */
    if (sort_comp(f, luaH_getint(a, u), luaH_getint(a, l)))  /* a[u] < a[l]? */
      swap(a, l, u);
  }
  else {
    int i;
    int m = l;
    swap(a, l, (l+u)/2);  /* put middle element as pivot (a[l]) */
    *(L->stack.top++) = *luaH_getint(a, l);  /* save pivot on stack (for GC) */
    for  (i=l+1; i<=u; i++) {
      /* invariant: a[l+1..m] < P <= a[m+1..i-1] */
      if (sort_comp(f, luaH_getint(a, i), L->stack.top-1)) {  /* a[i] < P? */
        m++;
        swap(a, m, i);
      }
    }
    L->stack.top--;  /* remove pivot from stack */
    swap(a, l, m);
    /* a[l..m-1] < a[m] <= a[m+1..u] */
    if (m-l < u-m) {  /* check which "half" is bigger */
      auxsort(a, l, m-1, f);  /* call recursively the smaller one */
      l = m+1; goto init;  /* auxsort(a, m+1, u, f); */
    }
    else {
      auxsort(a, m+1, u, f);
      u = m-1; goto init;  /* auxsort(a, l, m-1, f); */
    }
  }
}

static void luaB_sort (void) {
  lua_Object t = luaL_tablearg(1);
  int n = getnarg(t);
  Hash *a = avalue(luaA_Address(t));
  lua_Object func = lua_getparam(2);
  TObject *f = luaA_Address(func);
  luaL_arg_check(!f || lua_isfunction(func), 2, "function expected");
  auxsort(a, 1, n, f);
  lua_pushobject(t);
}


/*
** =======================================================
** some DEBUG functions
** =======================================================
*/
#ifdef DEBUG

static void mem_query (void)
{
  lua_pushnumber(totalmem);
  lua_pushnumber(numblocks);
}


static void countlist (void)
{
  char *s = luaL_check_string(1);
  GCnode *l = (s[0]=='t') ? L->roottable.next : (s[0]=='c') ? L->rootcl.next :
              (s[0]=='p') ? L->rootproto.next : L->rootglobal.next;
  int i=0;
  while (l) {
    i++;
    l = l->next;
  }
  lua_pushnumber(i);
}


static void testC (void)
{
#define getnum(s)	((*s++) - '0')
#define getname(s)	(nome[0] = *s++, nome)

  static int locks[10];
  lua_Object reg[10];
  char nome[2];
  char *s = luaL_check_string(1);
  nome[1] = 0;
  while (1) {
    switch (*s++) {
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
        lua_pushnumber(*(s-1) - '0');
        break;

      case 'c': reg[getnum(s)] = lua_createtable(); break;
      case 'C': { lua_CFunction f = lua_getcfunction(lua_getglobal(getname(s)));
                  lua_pushcclosure(f, getnum(s));
                  break;
                }
      case 'P': reg[getnum(s)] = lua_pop(); break;
      case 'g': { int n=getnum(s); reg[n]=lua_getglobal(getname(s)); break; }
      case 'G': { int n = getnum(s);
                  reg[n] = lua_rawgetglobal(getname(s));
                  break;
                }
      case 'l': locks[getnum(s)] = lua_ref(1); break;
      case 'L': locks[getnum(s)] = lua_ref(0); break;
      case 'r': { int n=getnum(s); reg[n]=lua_getref(locks[getnum(s)]); break; }
      case 'u': lua_unref(locks[getnum(s)]); break;
      case 'p': { int n = getnum(s); reg[n] = lua_getparam(getnum(s)); break; }
      case '=': lua_setglobal(getname(s)); break;
      case 's': lua_pushstring(getname(s)); break;
      case 'o': lua_pushobject(reg[getnum(s)]); break;
      case 'f': (lua_call)(getname(s)); break;
      case 'i': reg[getnum(s)] = lua_gettable(); break;
      case 'I': reg[getnum(s)] = lua_rawgettable(); break;
      case 't': lua_settable(); break;
      case 'T': lua_rawsettable(); break;
      default: luaL_verror("unknown command in `testC': %c", *(s-1));
    }
  if (*s == 0) return;
  if (*s++ != ' ') lua_error("missing ` ' between commands in `testC'");
  }
}

#endif


/*
** Internal functions
*/
static struct luaL_reg int_funcs[] = {
#ifdef LUA_COMPAT2_5
  {"setfallback", luaT_setfallback},
#endif
#ifdef DEBUG
  {"testC", testC},
  {"totalmem", mem_query},
  {"count", countlist},
#endif
  {"assert", luaI_assert},
  {"call", luaI_call},
  {"collectgarbage", luaI_collectgarbage},
  {"dofile", internaldofile},
  {"copytagmethods", copytagmethods},
  {"dostring", internaldostring},
  {"error", luaI_error},
  {"_ERRORMESSAGE", error_message},
  {"foreach", foreach},
  {"foreachi", foreachi},
  {"foreachvar", foreachvar},
  {"getn", getn},
  {"getglobal", getglobal},
  {"newtag", newtag},
  {"next", next},
  {"nextvar", nextvar},
  {"print", luaI_print},
  {"rawgetglobal", rawgetglobal},
  {"rawgettable", rawgettable},
  {"rawsetglobal", rawsetglobal},
  {"rawsettable", rawsettable},
  {"seterrormethod", seterrormethod},
  {"setglobal", setglobal},
  {"settagmethod", settagmethod},
  {"gettagmethod", gettagmethod},
  {"settag", settag},
  {"sort", luaB_sort},
  {"tonumber", luaB_tonumber},
  {"tostring", to_string},
  {"tag", luatag},
  {"type", luaI_type},
  {"_ALERT", luaB_message}
};


#define INTFUNCSIZE (sizeof(int_funcs)/sizeof(int_funcs[0]))


void luaB_predefine (void)
{
  /* pre-register mem error messages, to avoid loop when error arises */
  luaS_newfixedstring(tableEM);
  luaS_newfixedstring(memEM);
  luaL_openlib(int_funcs, (sizeof(int_funcs)/sizeof(int_funcs[0])));
  lua_pushstring(LUA_VERSION);
  lua_setglobal("_VERSION");
}

