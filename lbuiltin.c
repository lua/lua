/*
** $Id: lbuiltin.c,v 1.80 1999/12/02 16:24:45 roberto Exp roberto $
** Built-in functions
** See Copyright Notice in lua.h
*/


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LUA_REENTRANT

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


static void pushtagstring (lua_State *L, TaggedString *s) {
  ttype(L->top) = LUA_T_STRING;
  tsvalue(L->top) = s;
  incr_top;
}


static real getsize (const Hash *h) {
  real max = 0;
  int i = h->size;
  Node *n = h->node;
  while (i--) {
    if (ttype(key(n)) == LUA_T_NUMBER && 
        ttype(val(n)) != LUA_T_NIL &&
        nvalue(key(n)) > max)
      max = nvalue(key(n));
    n++;
  }
  return max;
}


static real getnarg (lua_State *L, const Hash *a) {
  TObject index;
  const TObject *value;
  /* value = table.n */
  ttype(&index) = LUA_T_STRING;
  tsvalue(&index) = luaS_new(L, "n");
  value = luaH_get(L, a, &index);
  return (ttype(value) == LUA_T_NUMBER) ? nvalue(value) : getsize(a);
}


static Hash *gettable (lua_State *L, int arg) {
  return avalue(luaL_tablearg(L, arg));
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
static void luaB_alert (lua_State *L) {
  fputs(luaL_check_string(L, 1), stderr);
}


/*
** Standard implementation of _ERRORMESSAGE.
** The library "iolib" redefines _ERRORMESSAGE for better error information.
*/
static void error_message (lua_State *L) {
  lua_Object al = lua_rawgetglobal(L, "_ALERT");
  if (lua_isfunction(L, al)) {  /* avoid error loop if _ALERT is not defined */
    char buff[600];
    sprintf(buff, "lua error: %.500s\n", luaL_check_string(L, 1));
    lua_pushstring(L, buff);
    lua_callfunction(L, al);
  }
}


/*
** If your system does not support "stdout", you can just remove this function.
** If you need, you can define your own "print" function, following this
** model but changing "fputs" to put the strings at a proper place
** (a console window or a log file, for instance).
*/
#ifndef MAXPRINT
#define MAXPRINT	40  /* arbitrary limit */
#endif

static void luaB_print (lua_State *L) {
  lua_Object args[MAXPRINT];
  lua_Object obj;
  int n = 0;
  int i;
  while ((obj = lua_getparam(L, n+1)) != LUA_NOOBJECT) {
    luaL_arg_check(L, n < MAXPRINT, n+1, "too many arguments");
    args[n++] = obj;
  }
  for (i=0; i<n; i++) {
    lua_pushobject(L, args[i]);
    if (lua_call(L, "tostring"))
      lua_error(L, "error in `tostring' called by `print'");
    obj = lua_getresult(L, 1);
    if (!lua_isstring(L, obj))
      lua_error(L, "`tostring' must return a string to `print'");
    if (i>0) fputs("\t", stdout);
    fputs(lua_getstring(L, obj), stdout);
  }
  fputs("\n", stdout);
}


static void luaB_tonumber (lua_State *L) {
  int base = luaL_opt_int(L, 2, 10);
  if (base == 10) {  /* standard conversion */
    lua_Object o = lua_getparam(L, 1);
    if (lua_isnumber(L, o)) lua_pushnumber(L, lua_getnumber(L, o));
    else lua_pushnil(L);  /* not a number */
  }
  else {
    const char *s1 = luaL_check_string(L, 1);
    char *s2;
    real n;
    luaL_arg_check(L, 0 <= base && base <= 36, 2, "base out of range");
    n = strtoul(s1, &s2, base);
    if (s1 == s2) return;  /* no valid digits: return nil */
    while (isspace((unsigned char)*s2)) s2++;  /* skip trailing spaces */
    if (*s2) return;  /* invalid trailing character: return nil */
    lua_pushnumber(L, n);
  }
}


static void luaB_error (lua_State *L) {
  lua_error(L, lua_getstring(L, lua_getparam(L, 1)));
}

static void luaB_setglobal (lua_State *L) {
  const char *n = luaL_check_string(L, 1);
  lua_Object value = luaL_nonnullarg(L, 2);
  lua_pushobject(L, value);
  lua_setglobal(L, n);
}

static void luaB_rawsetglobal (lua_State *L) {
  const char *n = luaL_check_string(L, 1);
  lua_Object value = luaL_nonnullarg(L, 2);
  lua_pushobject(L, value);
  lua_rawsetglobal(L, n);
}

static void luaB_getglobal (lua_State *L) {
  lua_pushobject(L, lua_getglobal(L, luaL_check_string(L, 1)));
}

static void luaB_rawgetglobal (lua_State *L) {
  lua_pushobject(L, lua_rawgetglobal(L, luaL_check_string(L, 1)));
}

static void luaB_tag (lua_State *L) {
  lua_pushnumber(L, lua_tag(L, lua_getparam(L, 1)));
}

static void luaB_settag (lua_State *L) {
  lua_Object o = luaL_tablearg(L, 1);
  lua_pushobject(L, o);
  lua_settag(L, luaL_check_int(L, 2));
  lua_pushobject(L, o);  /* return first argument */
}

static void luaB_newtag (lua_State *L) {
  lua_pushnumber(L, lua_newtag(L));
}

static void luaB_copytagmethods (lua_State *L) {
  lua_pushnumber(L, lua_copytagmethods(L, luaL_check_int(L, 1),
                                    luaL_check_int(L, 2)));
}

static void luaB_rawgettable (lua_State *L) {
  lua_pushobject(L, luaL_nonnullarg(L, 1));
  lua_pushobject(L, luaL_nonnullarg(L, 2));
  lua_pushobject(L, lua_rawgettable(L));
}

static void luaB_rawsettable (lua_State *L) {
  lua_pushobject(L, luaL_nonnullarg(L, 1));
  lua_pushobject(L, luaL_nonnullarg(L, 2));
  lua_pushobject(L, luaL_nonnullarg(L, 3));
  lua_rawsettable(L);
}

static void luaB_settagmethod (lua_State *L) {
  int tag = luaL_check_int(L, 1);
  const char *event = luaL_check_string(L, 2);
  lua_Object nf = luaL_nonnullarg(L, 3);
#ifndef LUA_COMPAT_GC
  if (strcmp(event, "gc") == 0 && tag != LUA_T_NIL)
    lua_error(L, "cannot set this tag method from Lua");
#endif
  lua_pushobject(L, nf);
  lua_pushobject(L, lua_settagmethod(L, tag, event));
}

static void luaB_gettagmethod (lua_State *L) {
  lua_pushobject(L, lua_gettagmethod(L, luaL_check_int(L, 1),
                                        luaL_check_string(L, 2)));
}

static void luaB_seterrormethod (lua_State *L) {
  lua_Object nf = luaL_functionarg(L, 1);
  lua_pushobject(L, nf);
  lua_pushobject(L, lua_seterrormethod(L));
}

static void luaB_collectgarbage (lua_State *L) {
  lua_pushnumber(L, lua_collectgarbage(L, luaL_opt_int(L, 1, 0)));
}


static void luaB_type (lua_State *L) {
  lua_Object o = luaL_nonnullarg(L, 1);
  lua_pushstring(L, lua_type(L, o));
}

/* }====================================================== */


/*
** {======================================================
** Functions that could use only the official API but
** do not, for efficiency.
** =======================================================
*/


static void passresults (lua_State *L) {
  L->Cstack.base = L->Cstack.lua2C;  /* position of first result */
  if (L->Cstack.num == 0)
    lua_pushuserdata(L, NULL);  /* at least one result to signal no errors */
}

static void luaB_dostring (lua_State *L) {
  long l;
  const char *s = luaL_check_lstr(L, 1, &l);
  if (*s == ID_CHUNK)
    lua_error(L, "`dostring' cannot run pre-compiled code");
  if (lua_dobuffer(L, s, l, luaL_opt_string(L, 2, s)) == 0)
    passresults(L);
  /* else return no value */
}


static void luaB_dofile (lua_State *L) {
  const char *fname = luaL_opt_string(L, 1, NULL);
  if (lua_dofile(L, fname) == 0)
    passresults(L);
  /* else return no value */
}


static void luaB_call (lua_State *L) {
  lua_Object f = luaL_nonnullarg(L, 1);
  const Hash *arg = gettable(L, 2);
  const char *options = luaL_opt_string(L, 3, "");
  lua_Object err = lua_getparam(L, 4);
  int narg = (int)getnarg(L, arg);
  int i, status;
  if (err != LUA_NOOBJECT) {  /* set new error method */
    lua_pushobject(L, err);
    err = lua_seterrormethod(L);
  }
  /* push arg[1...n] */
  luaD_checkstack(L, narg);
  for (i=0; i<narg; i++)
    *(L->top++) = *luaH_getint(L, arg, i+1);
  status = lua_callfunction(L, f);
  if (err != LUA_NOOBJECT) {  /* restore old error method */
    lua_pushobject(L, err);
    lua_seterrormethod(L);
  }
  if (status != 0) {  /* error in call? */
    if (strchr(options, 'x')) {
      lua_pushnil(L);
      return;  /* return nil to signal the error */
    }
    else
      lua_error(L, NULL);
  }
  else {  /* no errors */
    if (strchr(options, 'p')) {  /* pack results? */
      luaV_pack(L, L->Cstack.lua2C, L->Cstack.num, L->top);
      incr_top;
    }
    else
      L->Cstack.base = L->Cstack.lua2C;  /* position of first result */
  }
}


static void luaB_nextvar (lua_State *L) {
  lua_Object o = luaL_nonnullarg(L, 1);
  TaggedString *g;
  if (ttype(o) == LUA_T_NIL)
    g = NULL;
  else {
    luaL_arg_check(L, ttype(o) == LUA_T_STRING, 1, "variable name expected");
    g = tsvalue(o);
  }
  if (!luaA_nextvar(L, g))
    lua_pushnil(L);
}


static void luaB_next (lua_State *L) {
  const Hash *a = gettable(L, 1);
  lua_Object k = luaL_nonnullarg(L, 2);
  int i;  /* will get first element after `i' */
  if (ttype(k) == LUA_T_NIL)
    i = 0;  /* get first */
  else {
    i = luaH_pos(L, a, k)+1;
    luaL_arg_check(L, i != 0, 2, "key not found");
  }
  if (luaA_next(L, a, i) == 0)
    lua_pushnil(L);
}


static void luaB_tostring (lua_State *L) {
  lua_Object o = lua_getparam(L, 1);
  char buff[64];
  switch (ttype(o)) {
    case LUA_T_NUMBER:
      lua_pushstring(L, lua_getstring(L, o));
      return;
    case LUA_T_STRING:
      lua_pushobject(L, o);
      return;
    case LUA_T_ARRAY:
      sprintf(buff, "table: %p", o->value.a);
      break;
    case LUA_T_CLOSURE:
      sprintf(buff, "function: %p", o->value.cl);
      break;
    case LUA_T_PROTO:
      sprintf(buff, "function: %p", o->value.tf);
      break;
    case LUA_T_CPROTO:
      sprintf(buff, "function: %p", o->value.f);
      break;
    case LUA_T_USERDATA:
      sprintf(buff, "userdata: %p", o->value.ts->u.d.value);
      break;
    case LUA_T_NIL:
      lua_pushstring(L, "nil");
      return;
    default:
      LUA_INTERNALERROR(L, "invalid type");
  }
  lua_pushstring(L, buff);
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

static void luaB_assert (lua_State *L) {
  lua_Object p = lua_getparam(L, 1);
  if (p == LUA_NOOBJECT || lua_isnil(L, p))
    luaL_verror(L, "assertion failed!  %.90s", luaL_opt_string(L, 2, ""));
}


static void luaB_foreachi (lua_State *L) {
  const Hash *t = gettable(L, 1);
  int n = (int)getnarg(L, t);
  int i;
  lua_Object f = luaL_functionarg(L, 2);
  luaD_checkstack(L, 3);  /* for f, key, and val */
  for (i=1; i<=n; i++) {
    *(L->top++) = *f;
    ttype(L->top) = LUA_T_NUMBER; nvalue(L->top++) = i;
    *(L->top++) = *luaH_getint(L, t, i);
    luaD_call(L, L->top-3, 1);
    if (ttype(L->top-1) != LUA_T_NIL)
      return;
    L->top--;
  }
}


static void luaB_foreach (lua_State *L) {
  const Hash *a = gettable(L, 1);
  lua_Object f = luaL_functionarg(L, 2);
  int i;
  luaD_checkstack(L, 3);  /* for f, key, and val */
  for (i=0; i<a->size; i++) {
    const Node *nd = &(a->node[i]);
    if (ttype(val(nd)) != LUA_T_NIL) {
      *(L->top++) = *f;
      *(L->top++) = *key(nd);
      *(L->top++) = *val(nd);
      luaD_call(L, L->top-3, 1);
      if (ttype(L->top-1) != LUA_T_NIL)
        return;
      L->top--;  /* remove result */
    }
  }
}


static void luaB_foreachvar (lua_State *L) {
  lua_Object f = luaL_functionarg(L, 1);
  GlobalVar *gv;
  luaD_checkstack(L, 4);  /* for extra var name, f, var name, and globalval */
  for (gv = L->rootglobal; gv; gv = gv->next) {
    if (gv->value.ttype != LUA_T_NIL) {
      pushtagstring(L, gv->name);  /* keep (extra) name on stack to avoid GC */
      *(L->top++) = *f;
      pushtagstring(L, gv->name);
      *(L->top++) = gv->value;
      luaD_call(L, L->top-3, 1);
      if (ttype(L->top-1) != LUA_T_NIL) {
        L->top--;
        *(L->top-1) = *L->top;  /* remove extra name */
        return;
      }
      L->top-=2;  /* remove result and extra name */
    }
  }
}


static void luaB_getn (lua_State *L) {
  lua_pushnumber(L, getnarg(L, gettable(L, 1)));
}


static void luaB_tinsert (lua_State *L) {
  Hash *a = gettable(L, 1);
  lua_Object v = lua_getparam(L, 3);
  int n = (int)getnarg(L, a);
  int pos;
  if (v != LUA_NOOBJECT)
    pos = luaL_check_int(L, 2);
  else {  /* called with only 2 arguments */
    v = luaL_nonnullarg(L, 2);
    pos = n+1;
  }
  luaV_setn(L, a, n+1);  /* a.n = n+1 */
  for ( ;n>=pos; n--)
    luaH_move(L, a, n, n+1);  /* a[n+1] = a[n] */
  luaH_setint(L, a, pos, v);  /* a[pos] = v */
}


static void luaB_tremove (lua_State *L) {
  Hash *a = gettable(L, 1);
  int n = (int)getnarg(L, a);
  int pos = luaL_opt_int(L, 2, n);
  if (n <= 0) return;  /* table is "empty" */
  luaA_pushobject(L, luaH_getint(L, a, pos));  /* result = a[pos] */
  for ( ;pos<n; pos++)
    luaH_move(L, a, pos+1, pos);  /* a[pos] = a[pos+1] */
  luaV_setn(L, a, n-1);  /* a.n = n-1 */
  luaH_setint(L, a, n, &luaO_nilobject);  /* a[n] = nil */
}


/*
** {======================================================
** Quicksort
*/

static void swap (lua_State *L, Hash *a, int i, int j) {
  TObject temp;
  temp = *luaH_getint(L, a, i);
  luaH_move(L, a, j, i);
  luaH_setint(L, a, j, &temp);
}

static int sort_comp (lua_State *L, lua_Object f, const TObject *a,
                                                  const TObject *b) {
  /* notice: the caller (auxsort) must check stack space */
  if (f != LUA_NOOBJECT) {
    *(L->top) = *f;
    *(L->top+1) = *a;
    *(L->top+2) = *b;
    L->top += 3;
    luaD_call(L, L->top-3, 1);
  }
  else {  /* a < b? */
    *(L->top) = *a;
    *(L->top+1) = *b;
    luaV_comparison(L, L->top+2, LUA_T_NUMBER, LUA_T_NIL, LUA_T_NIL, IM_LT);
    L->top++;  /* result of comparison */
  }
  return ttype(--(L->top)) != LUA_T_NIL;
}

static void auxsort (lua_State *L, Hash *a, int l, int u, lua_Object f) {
  StkId P = L->top++;  /* temporary place for pivot */
  ttype(P) = LUA_T_NIL;
  while (l < u) {  /* for tail recursion */
    int i, j;
    /* sort elements a[l], a[(l+u)/2] and a[u] */
    if (sort_comp(L, f, luaH_getint(L, a, u), luaH_getint(L, a, l)))
      swap(L, a, l, u);  /* a[u]<a[l] */
    if (u-l == 1) break;  /* only 2 elements */
    i = (l+u)/2;
    *P = *luaH_getint(L, a, i);  /* P = a[i] */
    if (sort_comp(L, f, P, luaH_getint(L, a, l)))  /* a[i]<a[l]? */
      swap(L, a, l, i);
    else if (sort_comp(L, f, luaH_getint(L, a, u), P))  /* a[u]<a[i]? */
      swap(L, a, i, u);
    if (u-l == 2) break;  /* only 3 elements */
    *P = *luaH_getint(L, a, i);  /* save pivot on stack (GC) */
    swap(L, a, i, u-1);  /* put median element as pivot (a[u-1]) */
    /* a[l] <= P == a[u-1] <= a[u], only needs to sort from l+1 to u-2 */
    i = l; j = u-1;
    for (;;) {  /* invariant: a[l..i] <= P <= a[j..u] */
      /* repeat i++ until a[i] >= P */
      while (sort_comp(L, f, luaH_getint(L, a, ++i), P))
        if (i>u) lua_error(L, "invalid order function for sorting");
      /* repeat j-- until a[j] <= P */
      while (sort_comp(L, f, P, luaH_getint(L, a, --j)))
        if (j<l) lua_error(L, "invalid order function for sorting");
      if (j<i) break;
      swap(L, a, i, j);
    }
    swap(L, a, u-1, i);  /* swap pivot (a[u-1]) with a[i] */
    /* a[l..i-1] <= a[i] == P <= a[i+1..u] */
    /* adjust so that smaller "half" is in [j..i] and larger one in [l..u] */
    if (i-l < u-i) {
      j=l; i=i-1; l=i+2;
    }
    else {
      j=i+1; i=u; u=j-2;
    }
    auxsort(L, a, j, i, f);  /* call recursively the smaller one */
  }  /* repeat the routine for the larger one */
  L->top--;  /* remove pivot from stack */
}

static void luaB_sort (lua_State *L) {
  lua_Object t = lua_getparam(L, 1);
  Hash *a = gettable(L, 1);
  int n = (int)getnarg(L, a);
  lua_Object func = lua_getparam(L, 2);
  luaL_arg_check(L, func == LUA_NOOBJECT || lua_isfunction(L, func), 2,
                 "function expected");
  luaD_checkstack(L, 4);  /* for Pivot, f, a, b (sort_comp) */
  auxsort(L, a, 1, n, func);
  lua_pushobject(L, t);
}

/* }====================================================== */


/* }====================================================== */



#ifdef DEBUG
/*
** {======================================================
** some DEBUG functions
** (for internal debugging of the Lua implementation)
** =======================================================
*/

static void mem_query (lua_State *L) {
  lua_pushnumber(L, totalmem);
  lua_pushnumber(L, numblocks);
}


static void hash_query (lua_State *L) {
  lua_Object o = luaL_nonnullarg(L, 1);
  if (lua_getparam(L, 2) == LUA_NOOBJECT) {
    luaL_arg_check(L, ttype(o) == LUA_T_STRING, 1, "string expected");
    lua_pushnumber(L, tsvalue(o)->hash);
  }
  else {
    const Hash *t = avalue(luaL_tablearg(L, 2));
    lua_pushnumber(L, luaH_mainposition(L, t, o) - t->node);
  }
}


static void table_query (lua_State *L) {
  const Hash *t = avalue(luaL_tablearg(L, 1));
  int i = luaL_opt_int(L, 2, -1);
  if (i == -1) {
    lua_pushnumber(L, t->size);
    lua_pushnumber(L, t->firstfree - t->node);
  }
  else if (i < t->size) {
    luaA_pushobject(L, &t->node[i].key);
    luaA_pushobject(L, &t->node[i].val);
    if (t->node[i].next)
      lua_pushnumber(L, t->node[i].next - t->node);
  }
}


static void query_strings (lua_State *L) {
  int h = luaL_check_int(L, 1) - 1;
  int s = luaL_opt_int(L, 2, 0) - 1;
  if (s==-1) {
    if (h < NUM_HASHS) {
      lua_pushnumber(L, L->string_root[h].nuse);
      lua_pushnumber(L, L->string_root[h].size);
    }
  }
  else {
    TaggedString *ts = L->string_root[h].hash[s];
    for (ts = L->string_root[h].hash[s]; ts; ts = ts->nexthash) {
      if (ts->constindex == -1) lua_pushstring(L, "<USERDATA>");
      else lua_pushstring(L, ts->str);
    }
  }
}


static const char *delimits = " \t\n,;";

static void skip (const char **pc) {
  while (**pc != '\0' && strchr(delimits, **pc)) (*pc)++;
}

static int getnum (const char **pc) {
  int res = 0;
  skip(pc);
  while (isdigit(**pc)) res = res*10 + (*(*pc)++) - '0';
  return res;
}
  
static int getreg (lua_State *L, const char **pc) {
  skip(pc);
  if (*(*pc)++ != 'r') lua_error(L, "`testC' expecting a register");
  return getnum(pc);
}

static const char *getname (const char **pc) {
  static char buff[30];
  int i = 0;
  skip(pc);
  while (**pc != '\0' && !strchr(delimits, **pc))
    buff[i++] = *(*pc)++;
  buff[i] = '\0';
  return buff;
}


#define EQ(s1)	(strcmp(s1, inst) == 0)

static void testC (lua_State *L) {
  lua_Object reg[10];
  const char *pc = luaL_check_string(L, 1);
  for (;;) {
    const char *inst = getname(&pc);
    if EQ("") return;
    else if EQ("pushnum") {
      lua_pushnumber(L, getnum(&pc));
    }
    else if EQ("createtable") {
      reg[getreg(L, &pc)] = lua_createtable(L);
    }
    else if EQ("closure") {
      lua_CFunction f = lua_getcfunction(L, lua_getglobal(L, getname(&pc)));
      lua_pushcclosure(L, f, getnum(&pc));
    }
    else if EQ("pop") {
      reg[getreg(L, &pc)] = lua_pop(L);
    }
    else if EQ("getglobal") {
      int n = getreg(L, &pc);
      reg[n] = lua_getglobal(L, getname(&pc));
    }
    else if EQ("rawgetglobal") {
      int n = getreg(L, &pc);
      reg[n] = lua_rawgetglobal(L, getname(&pc));
    }
    else if EQ("ref") {
      lua_pushnumber(L, lua_ref(L, 0));
      reg[getreg(L, &pc)] = lua_pop(L);
    }
    else if EQ("reflock") {
      lua_pushnumber(L, lua_ref(L, 1));
      reg[getreg(L, &pc)] = lua_pop(L);
    }
    else if EQ("getref") {
      int n = getreg(L, &pc);
      reg[n] = lua_getref(L, (int)lua_getnumber(L, reg[getreg(L, &pc)]));
    }
    else if EQ("unref") {
      lua_unref(L, (int)lua_getnumber(L, reg[getreg(L, &pc)]));
    }
    else if (EQ("getparam") || EQ("getresult")) {
      int n = getreg(L, &pc);
      reg[n] = lua_getparam(L, getnum(&pc));
    }
    else if EQ("setglobal") {
      lua_setglobal(L, getname(&pc));
    }
    else if EQ("rawsetglobal") {
      lua_rawsetglobal(L, getname(&pc));
    }
    else if EQ("pushstring") {
      lua_pushstring(L, getname(&pc));
    }
    else if EQ("pushreg") {
      lua_pushobject(L, reg[getreg(L, &pc)]);
    }
    else if EQ("call") {
      lua_call(L, getname(&pc));
    }
    else if EQ("gettable") {
      reg[getreg(L, &pc)] = lua_gettable(L);
    }
    else if EQ("rawgettable") {
      reg[getreg(L, &pc)] = lua_rawgettable(L);
    }
    else if EQ("settable") {
      lua_settable(L);
    }
    else if EQ("rawsettable") {
      lua_rawsettable(L);
    }
    else if EQ("nextvar") {
      lua_pushstring(L, lua_nextvar(L, lua_getstring(L, reg[getreg(L, &pc)])));
    }
    else if EQ("next") {
      int n = getreg(L, &pc);
      n = lua_next(L, reg[n], (int)lua_getnumber(L, reg[getreg(L, &pc)]));
      lua_pushnumber(L, n);
    }
    else if EQ("equal") {
      int n1 = getreg(L, &pc);
      int n2 = getreg(L, &pc);
      lua_pushnumber(L, lua_equal(L, reg[n1], reg[n2]));
    }
    else if EQ("pushusertag") {
      int val = getreg(L, &pc);
      int tag = getreg(L, &pc);
      lua_pushusertag(L, (void *)(int)lua_getnumber(L, reg[val]),
                         lua_getnumber(L, reg[tag]));
    }
    else if EQ("udataval") {
      int n = getreg(L, &pc);
      lua_pushnumber(L, (int)lua_getuserdata(L, reg[getreg(L, &pc)]));
      reg[n] = lua_pop(L);
    }
    else if EQ("settagmethod") {
      int n = getreg(L, &pc);
      lua_settagmethod(L, lua_getnumber(L, reg[n]), getname(&pc));
    }
    else luaL_verror(L, "unknown command in `testC': %.20s", inst);
  }
}
                

/* }====================================================== */
#endif



static const struct luaL_reg builtin_funcs[] = {
#ifdef DEBUG
  {"hash", hash_query},
  {"querystr", query_strings},
  {"querytab", table_query},
  {"testC", testC},
  {"totalmem", mem_query},
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
  {"tag", luaB_tag},
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


void luaB_predefine (lua_State *L) {
  /* pre-register mem error messages, to avoid loop when error arises */
  luaS_newfixedstring(L, tableEM);
  luaS_newfixedstring(L, memEM);
  luaL_openl(L, builtin_funcs);
  lua_pushstring(L, LUA_VERSION);
  lua_setglobal(L, "_VERSION");
}

