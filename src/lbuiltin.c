/*
** $Id: lbuiltin.c,v 1.59 1999/06/17 17:04:03 roberto Exp $
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



/*
** {======================================================
** Auxiliary functions
** =======================================================
*/


static void pushtagstring (TaggedString *s) {
  TObject o;
  o.ttype = LUA_T_STRING;
  o.value.ts = s;
  luaA_pushobject(&o);
}


static real getsize (Hash *h) {
  real max = 0;
  int i;
  for (i = 0; i<nhash(h); i++) {
    Node *n = h->node+i;
    if (ttype(ref(n)) == LUA_T_NUMBER && 
        ttype(val(n)) != LUA_T_NIL &&
        nvalue(ref(n)) > max)
      max = nvalue(ref(n));
  }
  return max;
}


static real getnarg (Hash *a) {
  TObject index;
  TObject *value;
  /* value = table.n */
  ttype(&index) = LUA_T_STRING;
  tsvalue(&index) = luaS_new("n");
  value = luaH_get(a, &index);
  return (ttype(value) == LUA_T_NUMBER) ? nvalue(value) : getsize(a);
}


static Hash *gethash (int arg) {
  return avalue(luaA_Address(luaL_tablearg(arg)));
}

/* }====================================================== */


/*
** {======================================================
** Functions that use only the official API
** =======================================================
*/


/*
** If your system does not support "stderr", redefine this function, or
** redefine _ERRORMESSAGE so that it won't need _ALERT.
*/
static void luaB_alert (void) {
  fputs(luaL_check_string(1), stderr);
}


/*
** Standard implementation of _ERRORMESSAGE.
** The library "iolib" redefines _ERRORMESSAGE for better error information.
*/
static void error_message (void) {
  lua_Object al = lua_rawgetglobal("_ALERT");
  if (lua_isfunction(al)) {  /* avoid error loop if _ALERT is not defined */
    char buff[600];
    sprintf(buff, "lua error: %.500s\n", luaL_check_string(1));
    lua_pushstring(buff);
    lua_callfunction(al);
  }
}


/*
** If your system does not support "stdout", just remove this function.
** If you need, you can define your own "print" function, following this
** model but changing "fputs" to put the strings at a proper place
** (a console window or a log file, for instance).
*/
#ifndef MAXPRINT
#define MAXPRINT	40  /* arbitrary limit */
#endif

static void luaB_print (void) {
  lua_Object args[MAXPRINT];
  lua_Object obj;
  int n = 0;
  int i;
  while ((obj = lua_getparam(n+1)) != LUA_NOOBJECT) {
    luaL_arg_check(n < MAXPRINT, n+1, "too many arguments");
    args[n++] = obj;
  }
  for (i=0; i<n; i++) {
    lua_pushobject(args[i]);
    if (lua_call("tostring"))
      lua_error("error in `tostring' called by `print'");
    obj = lua_getresult(1);
    if (!lua_isstring(obj))
      lua_error("`tostring' must return a string to `print'");
    if (i>0) fputs("\t", stdout);
    fputs(lua_getstring(obj), stdout);
  }
  fputs("\n", stdout);
}


static void luaB_tonumber (void) {
  int base = luaL_opt_int(2, 10);
  if (base == 10) {  /* standard conversion */
    lua_Object o = lua_getparam(1);
    if (lua_isnumber(o)) lua_pushnumber(lua_getnumber(o));
    else lua_pushnil();  /* not a number */
  }
  else {
    char *s = luaL_check_string(1);
    long n;
    luaL_arg_check(0 <= base && base <= 36, 2, "base out of range");
    n = strtol(s, &s, base);
    while (isspace((unsigned char)*s)) s++;  /* skip trailing spaces */
    if (*s) lua_pushnil();  /* invalid format: return nil */
    else lua_pushnumber(n);
  }
}


static void luaB_error (void) {
  lua_error(lua_getstring(lua_getparam(1)));
}

static void luaB_setglobal (void) {
  char *n = luaL_check_string(1);
  lua_Object value = luaL_nonnullarg(2);
  lua_pushobject(value);
  lua_setglobal(n);
  lua_pushobject(value);  /* return given value */
}

static void luaB_rawsetglobal (void) {
  char *n = luaL_check_string(1);
  lua_Object value = luaL_nonnullarg(2);
  lua_pushobject(value);
  lua_rawsetglobal(n);
  lua_pushobject(value);  /* return given value */
}

static void luaB_getglobal (void) {
  lua_pushobject(lua_getglobal(luaL_check_string(1)));
}

static void luaB_rawgetglobal (void) {
  lua_pushobject(lua_rawgetglobal(luaL_check_string(1)));
}

static void luaB_luatag (void) {
  lua_pushnumber(lua_tag(lua_getparam(1)));
}

static void luaB_settag (void) {
  lua_Object o = luaL_tablearg(1);
  lua_pushobject(o);
  lua_settag(luaL_check_int(2));
  lua_pushobject(o);  /* return first argument */
}

static void luaB_newtag (void) {
  lua_pushnumber(lua_newtag());
}

static void luaB_copytagmethods (void) {
  lua_pushnumber(lua_copytagmethods(luaL_check_int(1),
                                    luaL_check_int(2)));
}

static void luaB_rawgettable (void) {
  lua_pushobject(luaL_nonnullarg(1));
  lua_pushobject(luaL_nonnullarg(2));
  lua_pushobject(lua_rawgettable());
}

static void luaB_rawsettable (void) {
  lua_pushobject(luaL_nonnullarg(1));
  lua_pushobject(luaL_nonnullarg(2));
  lua_pushobject(luaL_nonnullarg(3));
  lua_rawsettable();
}

static void luaB_settagmethod (void) {
  lua_Object nf = luaL_nonnullarg(3);
  lua_pushobject(nf);
  lua_pushobject(lua_settagmethod(luaL_check_int(1), luaL_check_string(2)));
}

static void luaB_gettagmethod (void) {
  lua_pushobject(lua_gettagmethod(luaL_check_int(1), luaL_check_string(2)));
}

static void luaB_seterrormethod (void) {
  lua_Object nf = luaL_functionarg(1);
  lua_pushobject(nf);
  lua_pushobject(lua_seterrormethod());
}

static void luaB_collectgarbage (void) {
  lua_pushnumber(lua_collectgarbage(luaL_opt_int(1, 0)));
}

/* }====================================================== */


/*
** {======================================================
** Functions that could use only the official API but
** do not, for efficiency.
** =======================================================
*/

static void luaB_dostring (void) {
  long l;
  char *s = luaL_check_lstr(1, &l);
  if (*s == ID_CHUNK)
    lua_error("`dostring' cannot run pre-compiled code");
  if (lua_dobuffer(s, l, luaL_opt_string(2, s)) == 0)
    if (luaA_passresults() == 0)
      lua_pushuserdata(NULL);  /* at least one result to signal no errors */
}


static void luaB_dofile (void) {
  char *fname = luaL_opt_string(1, NULL);
  if (lua_dofile(fname) == 0)
    if (luaA_passresults() == 0)
      lua_pushuserdata(NULL);  /* at least one result to signal no errors */
}


static void luaB_call (void) {
  lua_Object f = luaL_nonnullarg(1);
  Hash *arg = gethash(2);
  char *options = luaL_opt_string(3, "");
  lua_Object err = lua_getparam(4);
  int narg = (int)getnarg(arg);
  int i, status;
  if (err != LUA_NOOBJECT) {  /* set new error method */
    lua_pushobject(err);
    err = lua_seterrormethod();
  }
  /* push arg[1...n] */
  luaD_checkstack(narg);
  for (i=0; i<narg; i++)
    *(L->stack.top++) = *luaH_getint(arg, i+1);
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


static void luaB_nextvar (void) {
  TObject *o = luaA_Address(luaL_nonnullarg(1));
  TaggedString *g;
  if (ttype(o) == LUA_T_NIL)
    g = NULL;
  else {
    luaL_arg_check(ttype(o) == LUA_T_STRING, 1, "variable name expected");
    g = tsvalue(o);
  }
  if (!luaA_nextvar(g))
    lua_pushnil();
}


static void luaB_next (void) {
  Hash *a = gethash(1);
  TObject *k = luaA_Address(luaL_nonnullarg(2));
  int i = (ttype(k) == LUA_T_NIL) ? 0 : luaH_pos(a, k)+1;
  if (luaA_next(a, i) == 0)
    lua_pushnil();
}


static void luaB_tostring (void) {
  lua_Object obj = lua_getparam(1);
  TObject *o = luaA_Address(obj);
  char buff[64];
  switch (ttype(o)) {
    case LUA_T_NUMBER:
      lua_pushstring(lua_getstring(obj));
      return;
    case LUA_T_STRING:
      lua_pushobject(obj);
      return;
    case LUA_T_ARRAY:
      sprintf(buff, "table: %p", (void *)o->value.a);
      break;
    case LUA_T_CLOSURE:
      sprintf(buff, "function: %p", (void *)o->value.cl);
      break;
    case LUA_T_PROTO:
      sprintf(buff, "function: %p", (void *)o->value.tf);
      break;
    case LUA_T_CPROTO:
      sprintf(buff, "function: %p", (void *)o->value.f);
      break;
    case LUA_T_USERDATA:
      sprintf(buff, "userdata: %p", o->value.ts->u.d.v);
      break;
    case LUA_T_NIL:
      lua_pushstring("nil");
      return;
    default:
      LUA_INTERNALERROR("invalid type");
  }
  lua_pushstring(buff);
}


static void luaB_type (void) {
  lua_Object o = luaL_nonnullarg(1);
  lua_pushstring(luaO_typename(luaA_Address(o)));
  lua_pushnumber(lua_tag(o));
}

/* }====================================================== */



/*
** {======================================================
** "Extra" functions
** These functions can be written in Lua, so you can
**  delete them if you need a tiny Lua implementation.
** If you delete them, remove their entries in array
** "builtin_funcs".
** =======================================================
*/

static void luaB_assert (void) {
  lua_Object p = lua_getparam(1);
  if (p == LUA_NOOBJECT || lua_isnil(p))
    luaL_verror("assertion failed!  %.100s", luaL_opt_string(2, ""));
}


static void luaB_foreachi (void) {
  Hash *t = gethash(1);
  int i;
  int n = (int)getnarg(t);
  TObject f;
  /* 'f' cannot be a pointer to TObject, because it is on the stack, and the
     stack may be reallocated by the call. Moreover, some C compilers do not
     initialize structs, so we must do the assignment after the declaration */
  f = *luaA_Address(luaL_functionarg(2));
  luaD_checkstack(3);  /* for f, ref, and val */
  for (i=1; i<=n; i++) {
    *(L->stack.top++) = f;
    ttype(L->stack.top) = LUA_T_NUMBER; nvalue(L->stack.top++) = i;
    *(L->stack.top++) = *luaH_getint(t, i);
    luaD_calln(2, 1);
    if (ttype(L->stack.top-1) != LUA_T_NIL)
      return;
    L->stack.top--;
  }
}


static void luaB_foreach (void) {
  Hash *a = gethash(1);
  int i;
  TObject f;  /* see comment in 'foreachi' */
  f = *luaA_Address(luaL_functionarg(2));
  luaD_checkstack(3);  /* for f, ref, and val */
  for (i=0; i<a->nhash; i++) {
    Node *nd = &(a->node[i]);
    if (ttype(val(nd)) != LUA_T_NIL) {
      *(L->stack.top++) = f;
      *(L->stack.top++) = *ref(nd);
      *(L->stack.top++) = *val(nd);
      luaD_calln(2, 1);
      if (ttype(L->stack.top-1) != LUA_T_NIL)
        return;
      L->stack.top--;  /* remove result */
    }
  }
}


static void luaB_foreachvar (void) {
  GCnode *g;
  TObject f;  /* see comment in 'foreachi' */
  f = *luaA_Address(luaL_functionarg(1));
  luaD_checkstack(4);  /* for extra var name, f, var name, and globalval */
  for (g = L->rootglobal.next; g; g = g->next) {
    TaggedString *s = (TaggedString *)g;
    if (s->u.s.globalval.ttype != LUA_T_NIL) {
      pushtagstring(s);  /* keep (extra) s on stack to avoid GC */
      *(L->stack.top++) = f;
      pushtagstring(s);
      *(L->stack.top++) = s->u.s.globalval;
      luaD_calln(2, 1);
      if (ttype(L->stack.top-1) != LUA_T_NIL) {
        L->stack.top--;
        *(L->stack.top-1) = *L->stack.top;  /* remove extra s */
        return;
      }
      L->stack.top-=2;  /* remove result and extra s */
    }
  }
}


static void luaB_getn (void) {
  lua_pushnumber(getnarg(gethash(1)));
}


static void luaB_tinsert (void) {
  Hash *a = gethash(1);
  lua_Object v = lua_getparam(3);
  int n = (int)getnarg(a);
  int pos;
  if (v != LUA_NOOBJECT)
    pos = luaL_check_int(2);
  else {  /* called with only 2 arguments */
    v = luaL_nonnullarg(2);
    pos = n+1;
  }
  luaV_setn(a, n+1);  /* a.n = n+1 */
  for ( ;n>=pos; n--)
    luaH_move(a, n, n+1);  /* a[n+1] = a[n] */
  luaH_setint(a, pos, luaA_Address(v));  /* a[pos] = v */
}


static void luaB_tremove (void) {
  Hash *a = gethash(1);
  int n = (int)getnarg(a);
  int pos = luaL_opt_int(2, n);
  if (n <= 0) return;  /* table is "empty" */
  luaA_pushobject(luaH_getint(a, pos));  /* result = a[pos] */
  for ( ;pos<n; pos++)
    luaH_move(a, pos+1, pos);  /* a[pos] = a[pos+1] */
  luaV_setn(a, n-1);  /* a.n = n-1 */
  luaH_setint(a, n, &luaO_nilobject);  /* a[n] = nil */
}


/* {
** Quicksort
*/

static void swap (Hash *a, int i, int j) {
  TObject temp;
  temp = *luaH_getint(a, i);
  luaH_move(a, j, i);
  luaH_setint(a, j, &temp);
}

static int sort_comp (lua_Object f, TObject *a, TObject *b) {
  /* notice: the caller (auxsort) must check stack space */
  if (f != LUA_NOOBJECT) {
    *(L->stack.top) = *luaA_Address(f);
    *(L->stack.top+1) = *a;
    *(L->stack.top+2) = *b;
    L->stack.top += 3;
    luaD_calln(2, 1);
  }
  else {  /* a < b? */
    *(L->stack.top) = *a;
    *(L->stack.top+1) = *b;
    L->stack.top += 2;
    luaV_comparison(LUA_T_NUMBER, LUA_T_NIL, LUA_T_NIL, IM_LT);
  }
  return ttype(--(L->stack.top)) != LUA_T_NIL;
}

static void auxsort (Hash *a, int l, int u, lua_Object f) {
  while (l < u) {  /* for tail recursion */
    TObject *P;
    int i, j;
    /* sort elements a[l], a[(l+u)/2] and a[u] */
    if (sort_comp(f, luaH_getint(a, u), luaH_getint(a, l)))  /* a[l]>a[u]? */
      swap(a, l, u);
    if (u-l == 1) break;  /* only 2 elements */
    i = (l+u)/2;
    P = luaH_getint(a, i);
    if (sort_comp(f, P, luaH_getint(a, l)))  /* a[l]>a[i]? */
      swap(a, l, i);
    else if (sort_comp(f, luaH_getint(a, u), P))  /* a[i]>a[u]? */
      swap(a, i, u);
    if (u-l == 2) break;  /* only 3 elements */
    P = L->stack.top++;
    *P = *luaH_getint(a, i);  /* save pivot on stack (for GC) */
    swap(a, i, u-1);  /* put median element as pivot (a[u-1]) */
    /* a[l] <= P == a[u-1] <= a[u], only needs to sort from l+1 to u-2 */
    i = l; j = u-1;
    for  (;;) {
      /* invariant: a[l..i] <= P <= a[j..u] */
      while (sort_comp(f, luaH_getint(a, ++i), P))  /* stop when a[i] >= P */
        if (i>u) lua_error("invalid order function for sorting");
      while (sort_comp(f, P, luaH_getint(a, --j)))  /* stop when a[j] <= P */
        if (j<l) lua_error("invalid order function for sorting");
      if (j<i) break;
      swap(a, i, j);
    }
    swap(a, u-1, i);  /* swap pivot (a[u-1]) with a[i] */
    L->stack.top--;  /* remove pivot from stack */
    /* a[l..i-1] <= a[i] == P <= a[i+1..u] */
    /* adjust so that smaller "half" is in [j..i] and larger one in [l..u] */
    if (i-l < u-i) {
      j=l; i=i-1; l=i+2;
    }
    else {
      j=i+1; i=u; u=j-2;
    }
    auxsort(a, j, i, f);  /* call recursively the smaller one */
  }  /* repeat the routine for the larger one */
}

static void luaB_sort (void) {
  lua_Object t = lua_getparam(1);
  Hash *a = gethash(1);
  int n = (int)getnarg(a);
  lua_Object func = lua_getparam(2);
  luaL_arg_check(func == LUA_NOOBJECT || lua_isfunction(func), 2,
                 "function expected");
  luaD_checkstack(4);  /* for Pivot, f, a, b (sort_comp) */
  auxsort(a, 1, n, func);
  lua_pushobject(t);
}

/* }}===================================================== */


/*
** ====================================================== */



#ifdef DEBUG
/*
** {======================================================
** some DEBUG functions
** =======================================================
*/

static void mem_query (void) {
  lua_pushnumber(totalmem);
  lua_pushnumber(numblocks);
}


static void query_strings (void) {
  lua_pushnumber(L->string_root[luaL_check_int(1)].nuse);
}


static void countlist (void) {
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


static void testC (void) {
#define getnum(s)	((*s++) - '0')
#define getname(s)	(nome[0] = *s++, nome)

  static int locks[10];
  lua_Object reg[10];
  char nome[2];
  char *s = luaL_check_string(1);
  nome[1] = 0;
  for (;;) {
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
      case 'f': lua_call(getname(s)); break;
      case 'i': reg[getnum(s)] = lua_gettable(); break;
      case 'I': reg[getnum(s)] = lua_rawgettable(); break;
      case 't': lua_settable(); break;
      case 'T': lua_rawsettable(); break;
      case 'N' : lua_pushstring(lua_nextvar(lua_getstring(reg[getnum(s)])));
                 break;
      case 'n' : { int n=getnum(s);
                   n=lua_next(reg[n], (int)lua_getnumber(reg[getnum(s)]));
                   lua_pushnumber(n); break;
                 }
      default: luaL_verror("unknown command in `testC': %c", *(s-1));
    }
  if (*s == 0) return;
  if (*s++ != ' ') lua_error("missing ` ' between commands in `testC'");
  }
}

/* }====================================================== */
#endif



static struct luaL_reg builtin_funcs[] = {
#ifdef LUA_COMPAT2_5
  {"setfallback", luaT_setfallback},
#endif
#ifdef DEBUG
  {"testC", testC},
  {"totalmem", mem_query},
  {"count", countlist},
  {"querystr", query_strings},
#endif
  {"_ALERT", luaB_alert},
  {"_ERRORMESSAGE", error_message},
  {"call", luaB_call},
  {"collectgarbage", luaB_collectgarbage},
  {"copytagmethods", luaB_copytagmethods},
  {"dofile", luaB_dofile},
  {"dostring", luaB_dostring},
  {"error", luaB_error},
  {"getglobal", luaB_getglobal},
  {"gettagmethod", luaB_gettagmethod},
  {"newtag", luaB_newtag},
  {"next", luaB_next},
  {"nextvar", luaB_nextvar},
  {"print", luaB_print},
  {"rawgetglobal", luaB_rawgetglobal},
  {"rawgettable", luaB_rawgettable},
  {"rawsetglobal", luaB_rawsetglobal},
  {"rawsettable", luaB_rawsettable},
  {"seterrormethod", luaB_seterrormethod},
  {"setglobal", luaB_setglobal},
  {"settag", luaB_settag},
  {"settagmethod", luaB_settagmethod},
  {"tag", luaB_luatag},
  {"tonumber", luaB_tonumber},
  {"tostring", luaB_tostring},
  {"type", luaB_type},
  /* "Extra" functions */
  {"assert", luaB_assert},
  {"foreach", luaB_foreach},
  {"foreachi", luaB_foreachi},
  {"foreachvar", luaB_foreachvar},
  {"getn", luaB_getn},
  {"sort", luaB_sort},
  {"tinsert", luaB_tinsert},
  {"tremove", luaB_tremove}
};


#define INTFUNCSIZE (sizeof(builtin_funcs)/sizeof(builtin_funcs[0]))


void luaB_predefine (void) {
  /* pre-register mem error messages, to avoid loop when error arises */
  luaS_newfixedstring(tableEM);
  luaS_newfixedstring(memEM);
  luaL_openlib(builtin_funcs, (sizeof(builtin_funcs)/sizeof(builtin_funcs[0])));
  lua_pushstring(LUA_VERSION);
  lua_setglobal("_VERSION");
}

