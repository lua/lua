/*
** $Id: lbuiltin.c,v 1.112 2000/06/02 19:08:56 roberto Exp roberto $
** Built-in functions
** See Copyright Notice in lua.h
*/


/*
** =========================================================================
** All built-in functions are public (i.e. not static) and are named luaB_f,
** where f is the function name in Lua. So, if you do not need all these
** functions, you may register manually only the ones that you need.
** =========================================================================
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
** function defined in ltests.c, to open some internal-test functions
*/
void luaB_opentests (lua_State *L);



/*
** {======================================================
** Auxiliary functions
** =======================================================
*/


static Number getsize (const Hash *h) {
  Number max = 0;
  int i = h->size;
  Node *n = h->node;
  while (i--) {
    if (ttype(key(n)) == TAG_NUMBER && 
        ttype(val(n)) != TAG_NIL &&
        nvalue(key(n)) > max)
      max = nvalue(key(n));
    n++;
  }
  return max;
}


static Number getnarg (lua_State *L, const Hash *a) {
  const TObject *value = luaH_getstr(a, luaS_new(L, "n"));  /* value = a.n */
  return (ttype(value) == TAG_NUMBER) ? nvalue(value) : getsize(a);
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
** If your system does not support `stderr', redefine this function, or
** redefine _ERRORMESSAGE so that it won't need _ALERT.
*/
void luaB__ALERT (lua_State *L) {
  fputs(luaL_check_string(L, 1), stderr);
}


/*
** Standard implementation of _ERRORMESSAGE.
** The library `liolib' redefines _ERRORMESSAGE for better error information.
*/
void luaB__ERRORMESSAGE (lua_State *L) {
  lua_Object al;
  lua_pushglobaltable(L);
  lua_pushstring(L, LUA_ALERT);
  al = lua_rawget(L);
  if (lua_isfunction(L, al)) {  /* avoid error loop if _ALERT is not defined */
    const char *s = luaL_check_string(L, 1);
    char *buff = luaL_openspace(L, strlen(s)+sizeof("error: \n"));
    strcpy(buff, "error: "); strcat(buff, s); strcat(buff, "\n");
    lua_pushstring(L, buff);
    lua_callfunction(L, al);
  }
}


/*
** If your system does not support `stdout', you can just remove this function.
** If you need, you can define your own `print' function, following this
** model but changing `fputs' to put the strings at a proper place
** (a console window or a log file, for instance).
*/
void luaB_print (lua_State *L) {
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


void luaB_tonumber (lua_State *L) {
  int base = luaL_opt_int(L, 2, 10);
  if (base == 10) {  /* standard conversion */
    lua_Object o = luaL_nonnullarg(L, 1);
    if (lua_isnumber(L, o)) {
      lua_pushnumber(L, lua_getnumber(L, o));
      return;
    }
  }
  else {
    const char *s1 = luaL_check_string(L, 1);
    char *s2;
    Number n;
    luaL_arg_check(L, 2 <= base && base <= 36, 2, "base out of range");
    n = strtoul(s1, &s2, base);
    if (s1 != s2) {  /* at least one valid digit? */
      while (isspace((unsigned char)*s2)) s2++;  /* skip trailing spaces */
      if (*s2 == '\0') {  /* no invalid trailing characters? */
        lua_pushnumber(L, n);
        return;
      }
    }
  }
  lua_pushnil(L);  /* else not a number */
}


void luaB_error (lua_State *L) {
  lua_error(L, luaL_opt_string(L, 1, NULL));
}

void luaB_setglobal (lua_State *L) {
  const char *name = luaL_check_string(L, 1);
  lua_Object value = luaL_nonnullarg(L, 2);
  lua_pushobject(L, value);
  lua_setglobal(L, name);
}

void luaB_getglobal (lua_State *L) {
  lua_pushobject(L, lua_getglobal(L, luaL_check_string(L, 1)));
}

void luaB_tag (lua_State *L) {
  lua_pushnumber(L, lua_tag(L, luaL_nonnullarg(L, 1)));
}

void luaB_settag (lua_State *L) {
  lua_Object o = luaL_tablearg(L, 1);
  lua_pushobject(L, o);
  lua_settag(L, luaL_check_int(L, 2));
  lua_pushobject(L, o);  /* return first argument */
}

void luaB_newtag (lua_State *L) {
  lua_pushnumber(L, lua_newtag(L));
}

void luaB_copytagmethods (lua_State *L) {
  lua_pushnumber(L, lua_copytagmethods(L, luaL_check_int(L, 1),
                                          luaL_check_int(L, 2)));
}

void luaB_globals (lua_State *L) {
  lua_pushglobaltable(L);
  if (lua_getparam(L, 1) != LUA_NOOBJECT)
    lua_setglobaltable(L, luaL_tablearg(L, 1));
}

void luaB_rawget (lua_State *L) {
  lua_pushobject(L, luaL_nonnullarg(L, 1));
  lua_pushobject(L, luaL_nonnullarg(L, 2));
  lua_pushobject(L, lua_rawget(L));
}

void luaB_rawset (lua_State *L) {
  lua_pushobject(L, luaL_nonnullarg(L, 1));
  lua_pushobject(L, luaL_nonnullarg(L, 2));
  lua_pushobject(L, luaL_nonnullarg(L, 3));
  lua_rawset(L);
}

void luaB_settagmethod (lua_State *L) {
  int tag = luaL_check_int(L, 1);
  const char *event = luaL_check_string(L, 2);
  lua_Object nf = luaL_nonnullarg(L, 3);
  luaL_arg_check(L, lua_isnil(L, nf) || lua_isfunction(L, nf), 3,
                 "function or nil expected");
  if (strcmp(event, "gc") == 0 && tag != TAG_NIL)
    lua_error(L, "deprecated use: cannot set the `gc' tag method from Lua");
  lua_pushobject(L, nf);
  lua_pushobject(L, lua_settagmethod(L, tag, event));
}

void luaB_gettagmethod (lua_State *L) {
  lua_pushobject(L, lua_gettagmethod(L, luaL_check_int(L, 1),
                                        luaL_check_string(L, 2)));
}


void luaB_collectgarbage (lua_State *L) {
  lua_pushnumber(L, lua_collectgarbage(L, luaL_opt_int(L, 1, 0)));
}


void luaB_type (lua_State *L) {
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

void luaB_dostring (lua_State *L) {
  size_t l;
  const char *s = luaL_check_lstr(L, 1, &l);
  if (*s == ID_CHUNK)
    lua_error(L, "`dostring' cannot run pre-compiled code");
  if (lua_dobuffer(L, s, l, luaL_opt_string(L, 2, s)) == 0)
    passresults(L);
  else
    lua_pushnil(L);
}


void luaB_dofile (lua_State *L) {
  const char *fname = luaL_opt_string(L, 1, NULL);
  if (lua_dofile(L, fname) == 0)
    passresults(L);
  else
    lua_pushnil(L);
}


void luaB_call (lua_State *L) {
  lua_Object f = luaL_nonnullarg(L, 1);
  const Hash *arg = gettable(L, 2);
  const char *options = luaL_opt_string(L, 3, "");
  lua_Object err = lua_getparam(L, 4);
  int narg = (int)getnarg(L, arg);
  int i, status;
  if (err != LUA_NOOBJECT) {  /* set new error method */
    lua_Object oldem = lua_getglobal(L, LUA_ERRORMESSAGE);
    lua_pushobject(L, err);
    lua_setglobal(L, LUA_ERRORMESSAGE);
    err = oldem;
  }
  /* push arg[1...n] */
  luaD_checkstack(L, narg);
  for (i=0; i<narg; i++)
    *(L->top++) = *luaH_getnum(arg, i+1);
  status = lua_callfunction(L, f);
  if (err != LUA_NOOBJECT) {  /* restore old error method */
    lua_pushobject(L, err);
    lua_setglobal(L, LUA_ERRORMESSAGE);
  }
  if (status != 0) {  /* error in call? */
    if (strchr(options, 'x')) {
      lua_pushnil(L);
      return;  /* return nil to signal the error */
    }
    else
      lua_error(L, NULL);  /* propagate error without additional messages */
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


void luaB_next (lua_State *L) {
  const Hash *a = gettable(L, 1);
  lua_Object k = lua_getparam(L, 2);
  int i;  /* `luaA_next' gets first element after `i' */
  if (k == LUA_NOOBJECT || ttype(k) == TAG_NIL)
    i = 0;  /* get first */
  else {
    i = luaH_pos(L, a, k)+1;
    luaL_arg_check(L, i != 0, 2, "key not found");
  }
  if (luaA_next(L, a, i) == 0)
    lua_pushnil(L);
}


void luaB_tostring (lua_State *L) {
  lua_Object o = luaL_nonnullarg(L, 1);
  char buff[64];
  switch (ttype(o)) {
    case TAG_NUMBER:
      lua_pushstring(L, lua_getstring(L, o));
      return;
    case TAG_STRING:
      lua_pushobject(L, o);
      return;
    case TAG_TABLE:
      sprintf(buff, "table: %p", o->value.a);
      break;
    case TAG_LCLOSURE:  case TAG_CCLOSURE:
      sprintf(buff, "function: %p", o->value.cl);
      break;
    case TAG_USERDATA:
      sprintf(buff, "userdata: %p(%d)", o->value.ts->u.d.value,
                                        o->value.ts->u.d.tag);
      break;
    case TAG_NIL:
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

void luaB_assert (lua_State *L) {
  lua_Object p = luaL_nonnullarg(L, 1);
  if (lua_isnil(L, p))
    luaL_verror(L, "assertion failed!  %.90s", luaL_opt_string(L, 2, ""));
}


void luaB_getn (lua_State *L) {
  lua_pushnumber(L, getnarg(L, gettable(L, 1)));
}


void luaB_tinsert (lua_State *L) {
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
  for (; n>=pos; n--)
    luaH_move(L, a, n, n+1);  /* a[n+1] = a[n] */
  luaH_setint(L, a, pos, v);  /* a[pos] = v */
}


void luaB_tremove (lua_State *L) {
  Hash *a = gettable(L, 1);
  int n = (int)getnarg(L, a);
  int pos = luaL_opt_int(L, 2, n);
  if (n <= 0) return;  /* table is "empty" */
  luaA_pushobject(L, luaH_getnum(a, pos));  /* result = a[pos] */
  for ( ;pos<n; pos++)
    luaH_move(L, a, pos+1, pos);  /* a[pos] = a[pos+1] */
  luaV_setn(L, a, n-1);  /* a.n = n-1 */
  luaH_setint(L, a, n, &luaO_nilobject);  /* a[n] = nil */
}


static void luaB_foreachi (lua_State *L) {
  const Hash *t = gettable(L, 1);
  int n = (int)getnarg(L, t);
  int i;
  lua_Object f = luaL_functionarg(L, 2);
  luaD_checkstack(L, 3);  /* for f, key, and val */
  for (i=1; i<=n; i++) {
    *(L->top++) = *f;
    ttype(L->top) = TAG_NUMBER; nvalue(L->top++) = i;
    *(L->top++) = *luaH_getnum(t, i);
    luaD_call(L, L->top-3, 1);
    if (ttype(L->top-1) != TAG_NIL)
      return;
    L->top--;  /* remove nil result */
  }
}


static void luaB_foreach (lua_State *L) {
  const Hash *a = gettable(L, 1);
  lua_Object f = luaL_functionarg(L, 2);
  int i;
  luaD_checkstack(L, 3);  /* for f, key, and val */
  for (i=0; i<a->size; i++) {
    const Node *nd = &(a->node[i]);
    if (ttype(val(nd)) != TAG_NIL) {
      *(L->top++) = *f;
      *(L->top++) = *key(nd);
      *(L->top++) = *val(nd);
      luaD_call(L, L->top-3, 1);
      if (ttype(L->top-1) != TAG_NIL)
        return;
      L->top--;  /* remove result */
    }
  }
}


/*
** {======================================================
** Quicksort
** (based on `Algorithms in MODULA-3', Robert Sedgewick;
**  Addison-Wesley, 1993.)
*/

static void swap (lua_State *L, Hash *a, int i, int j) {
  TObject temp;
  temp = *luaH_getnum(a, i);
  luaH_move(L, a, j, i);
  luaH_setint(L, a, j, &temp);
}

static int sort_comp (lua_State *L, lua_Object f, const TObject *a,
                                                  const TObject *b) {
  /* WARNING: the caller (auxsort) must ensure stack space */
  if (f != LUA_NOOBJECT) {
    *(L->top) = *f;
    *(L->top+1) = *a;
    *(L->top+2) = *b;
    L->top += 3;
    luaD_call(L, L->top-3, 1);
    L->top--;
    return (ttype(L->top) != TAG_NIL);
  }
  else  /* a < b? */
    return luaV_lessthan(L, a, b, L->top);
}

static void auxsort (lua_State *L, Hash *a, int l, int u, lua_Object f) {
  StkId P = L->top++;  /* temporary place for pivot */
  ttype(P) = TAG_NIL;
  while (l < u) {  /* for tail recursion */
    int i, j;
    /* sort elements a[l], a[(l+u)/2] and a[u] */
    if (sort_comp(L, f, luaH_getnum(a, u), luaH_getnum(a, l)))
      swap(L, a, l, u);  /* a[u]<a[l] */
    if (u-l == 1) break;  /* only 2 elements */
    i = (l+u)/2;
    *P = *luaH_getnum(a, i);  /* P = a[i] */
    if (sort_comp(L, f, P, luaH_getnum(a, l)))  /* a[i]<a[l]? */
      swap(L, a, l, i);
    else if (sort_comp(L, f, luaH_getnum(a, u), P))  /* a[u]<a[i]? */
      swap(L, a, i, u);
    if (u-l == 2) break;  /* only 3 elements */
    *P = *luaH_getnum(a, i);  /* save pivot on stack (GC) */
    swap(L, a, i, u-1);  /* put median element as pivot (a[u-1]) */
    /* a[l] <= P == a[u-1] <= a[u], only needs to sort from l+1 to u-2 */
    i = l; j = u-1;
    for (;;) {  /* invariant: a[l..i] <= P <= a[j..u] */
      /* repeat i++ until a[i] >= P */
      while (sort_comp(L, f, luaH_getnum(a, ++i), P))
        if (i>u) lua_error(L, "invalid order function for sorting");
      /* repeat j-- until a[j] <= P */
      while (sort_comp(L, f, P, luaH_getnum(a, --j)))
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

void luaB_sort (lua_State *L) {
  Hash *a = gettable(L, 1);
  int n = (int)getnarg(L, a);
  lua_Object func = lua_getparam(L, 2);
  luaL_arg_check(L, func == LUA_NOOBJECT || lua_isfunction(L, func), 2,
                 "function expected");
  luaD_checkstack(L, 4);  /* for pivot, f, a, b (sort_comp) */
  auxsort(L, a, 1, n, func);
}

/* }====================================================== */


/* }====================================================== */



/*
** {======================================================
** Deprecated functions to manipulate global environment:
** some of them can be simulated through table operations
** over the global table.
** =======================================================
*/


#ifdef LUA_DEPRECATETFUNCS

#define num_deprecated	4

static const struct luaL_reg deprecated_global_funcs[num_deprecated] = {
  {"foreachvar", luaB_foreach},
  {"nextvar", luaB_next},
  {"rawgetglobal", luaB_rawget},
  {"rawsetglobal", luaB_rawset}
};


static const struct luaL_reg other_deprecated_global_funcs[] = {
  {"rawgettable", luaB_rawget},
  {"rawsettable", luaB_rawset}
};



static void deprecated_funcs (lua_State *L) {
  TObject gt;
  int i;
  ttype(&gt) = TAG_TABLE;
  avalue(&gt) = L->gt;
  for (i=0; i<num_deprecated; i++) {
    lua_pushobject(L, &gt);
    lua_pushcclosure(L, deprecated_global_funcs[i].func, 1);
    lua_setglobal(L, deprecated_global_funcs[i].name);
  }
  luaL_openl(L, other_deprecated_global_funcs);
}

#else

/*
** gives an explicit error in any attempt to call an obsolet function
*/
static void obsolete_func (lua_State *L) {
  luaL_verror(L, "function `%.20s' is obsolete", luaL_check_string(L, 1));
}


#define num_deprecated	6

static const char *const obsolete_names [num_deprecated] = {
  "foreachvar", "nextvar", "rawgetglobal",
  "rawgettable", "rawsetglobal", "rawsettable"
};


static void deprecated_funcs (lua_State *L) {
  int i;
  for (i=0; i<num_deprecated; i++) {
    lua_pushstring(L, obsolete_names[i]);
    lua_pushcclosure(L, obsolete_func, 1);
    lua_setglobal(L, obsolete_names[i]);
  }
}

#endif

/* }====================================================== */

static const struct luaL_reg builtin_funcs[] = {
  {LUA_ALERT, luaB__ALERT},
  {LUA_ERRORMESSAGE, luaB__ERRORMESSAGE},
  {"call", luaB_call},
  {"collectgarbage", luaB_collectgarbage},
  {"copytagmethods", luaB_copytagmethods},
  {"dofile", luaB_dofile},
  {"dostring", luaB_dostring},
  {"error", luaB_error},
  {"foreach", luaB_foreach},
  {"foreachi", luaB_foreachi},
  {"getglobal", luaB_getglobal},
  {"gettagmethod", luaB_gettagmethod},
  {"globals", luaB_globals},
  {"newtag", luaB_newtag},
  {"next", luaB_next},
  {"print", luaB_print},
  {"rawget", luaB_rawget},
  {"rawset", luaB_rawset},
  {"setglobal", luaB_setglobal},
  {"settag", luaB_settag},
  {"settagmethod", luaB_settagmethod},
  {"tag", luaB_tag},
  {"tonumber", luaB_tonumber},
  {"tostring", luaB_tostring},
  {"type", luaB_type},
/* "Extra" functions */
  {"assert", luaB_assert},
  {"getn", luaB_getn},
  {"sort", luaB_sort},
  {"tinsert", luaB_tinsert},
  {"tremove", luaB_tremove}
};



void luaB_predefine (lua_State *L) {
  /* pre-register mem error messages, to avoid loop when error arises */
  luaS_newfixed(L, memEM);
  luaL_openl(L, builtin_funcs);
#ifdef DEBUG
  luaB_opentests(L);  /* internal test functions */
#endif
  lua_pushstring(L, LUA_VERSION);
  lua_setglobal(L, "_VERSION");
  deprecated_funcs(L);
}

