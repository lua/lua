/*
** $Id: lbuiltin.c,v 1.126 2000/08/31 16:52:06 roberto Exp roberto $
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

#include "lua.h"

#include "lauxlib.h"
#include "lbuiltin.h"



/*
** function defined in ltests.c, to open some internal-test functions
*/
void luaB_opentests (lua_State *L);




/*
** {======================================================
** Functions that use only the official API
** =======================================================
*/


/*
** If your system does not support `stderr', redefine this function, or
** redefine _ERRORMESSAGE so that it won't need _ALERT.
*/
int luaB__ALERT (lua_State *L) {
  fputs(luaL_check_string(L, 1), stderr);
  return 0;
}


/*
** Standard implementation of _ERRORMESSAGE.
** The library `liolib' redefines _ERRORMESSAGE for better error information.
*/
int luaB__ERRORMESSAGE (lua_State *L) {
  lua_getglobals(L);
  lua_pushstring(L, LUA_ALERT);
  lua_rawget(L);
  if (lua_isfunction(L, -1)) {  /* avoid error loop if _ALERT is not defined */
    const char *s = luaL_check_string(L, 1);
    char *buff = luaL_openspace(L, strlen(s)+sizeof("error: \n"));
    strcpy(buff, "error: "); strcat(buff, s); strcat(buff, "\n");
    lua_pushobject(L, -1);  /* function to be called */
    lua_pushstring(L, buff);
    lua_call(L, 1, 0);
  }
  return 0;
}


/*
** If your system does not support `stdout', you can just remove this function.
** If you need, you can define your own `print' function, following this
** model but changing `fputs' to put the strings at a proper place
** (a console window or a log file, for instance).
*/
int luaB_print (lua_State *L) {
  int n = lua_gettop(L);  /* number of arguments */
  int i;
  lua_getglobal(L, "tostring");
  for (i=1; i<=n; i++) {
    const char *s;
    lua_pushobject(L, -1);  /* function to be called */
    lua_pushobject(L, i);
    if (lua_call(L, 1, 1) != 0)
      lua_error(L, "error in `tostring' called by `print'");
    s = lua_tostring(L, -1);  /* get result */
    if (s == NULL)
      lua_error(L, "`tostring' must return a string to `print'");
    if (i>1) fputs("\t", stdout);
    fputs(s, stdout);
    lua_pop(L, 1);  /* pop result */
  }
  fputs("\n", stdout);
  return 0;
}


int luaB_tonumber (lua_State *L) {
  int base = luaL_opt_int(L, 2, 10);
  if (base == 10) {  /* standard conversion */
    luaL_checktype(L, 1, "any");
    if (lua_isnumber(L, 1)) {
      lua_pushnumber(L, lua_tonumber(L, 1));
      return 1;
    }
  }
  else {
    const char *s1 = luaL_check_string(L, 1);
    char *s2;
    unsigned long n;
    luaL_arg_check(L, 2 <= base && base <= 36, 2, "base out of range");
    n = strtoul(s1, &s2, base);
    if (s1 != s2) {  /* at least one valid digit? */
      while (isspace((unsigned char)*s2)) s2++;  /* skip trailing spaces */
      if (*s2 == '\0') {  /* no invalid trailing characters? */
        lua_pushnumber(L, n);
        return 1;
      }
    }
  }
  lua_pushnil(L);  /* else not a number */
  return 1;
}


int luaB_error (lua_State *L) {
  lua_error(L, luaL_opt_string(L, 1, NULL));
  return 0;  /* to avoid errors */
}

int luaB_setglobal (lua_State *L) {
  luaL_checktype(L, 2, "any");
  lua_setglobal(L, luaL_check_string(L, 1));
  return 0;
}

int luaB_getglobal (lua_State *L) {
  lua_getglobal(L, luaL_check_string(L, 1));
  return 1;
}

int luaB_tag (lua_State *L) {
  luaL_checktype(L, 1, "any");
  lua_pushnumber(L, lua_tag(L, 1));
  return 1;
}

int luaB_settag (lua_State *L) {
  luaL_checktype(L, 1, "table");
  lua_pushobject(L, 1);  /* push table */
  lua_settag(L, luaL_check_int(L, 2));
  lua_pushobject(L, 1);  /* return first argument */
  return 1;
}

int luaB_newtag (lua_State *L) {
  lua_pushnumber(L, lua_newtag(L));
  return 1;
}

int luaB_copytagmethods (lua_State *L) {
  lua_pushnumber(L, lua_copytagmethods(L, luaL_check_int(L, 1),
                                          luaL_check_int(L, 2)));
  return 1;
}

int luaB_globals (lua_State *L) {
  lua_getglobals(L);  /* value to be returned */
  if (!lua_isnull(L, 1)) {
    luaL_checktype(L, 1, "table");
    lua_pushobject(L, 1);  /* new table of globals */
    lua_setglobals(L);
  }
  return 1;
}

int luaB_rawget (lua_State *L) {
  luaL_checktype(L, 1, "table");
  luaL_checktype(L, 2, "any");
  lua_rawget(L);
  return 1;
}

int luaB_rawset (lua_State *L) {
  luaL_checktype(L, 1, "table");
  luaL_checktype(L, 2, "any");
  luaL_checktype(L, 3, "any");
  lua_rawset(L);
  return 1;
}

int luaB_settagmethod (lua_State *L) {
  int tag = (int)luaL_check_int(L, 1);
  const char *event = luaL_check_string(L, 2);
  luaL_arg_check(L, lua_isfunction(L, 3) || lua_isnil(L, 3), 3,
                 "function or nil expected");
  lua_pushnil(L);  /* to get its tag */
  if (strcmp(event, "gc") == 0 && tag != lua_tag(L, -1))
    lua_error(L, "deprecated use: cannot set the `gc' tag method from Lua");
  lua_pop(L, 1);  /* remove the nil */
  lua_settagmethod(L, tag, event);
  return 1;
}

int luaB_gettagmethod (lua_State *L) {
  lua_gettagmethod(L, luaL_check_int(L, 1), luaL_check_string(L, 2));
  return 1;
}


int luaB_collectgarbage (lua_State *L) {
  lua_pushnumber(L, lua_collectgarbage(L, luaL_opt_int(L, 1, 0)));
  return 1;
}


int luaB_type (lua_State *L) {
  luaL_checktype(L, 1, "any");
  lua_pushstring(L, lua_type(L, 1));
  return 1;
}


int luaB_next (lua_State *L) {
  luaL_checktype(L, 1, "table");
  lua_settop(L, 2);  /* create a 2nd argument if there isn't one */
  if (lua_next(L))
    return 2;
  else {
    lua_pushnil(L);
    return 1;
  }
}


static int passresults (lua_State *L, int status, int oldtop) {
  if (status == 0) {
    int nresults = lua_gettop(L) - oldtop;
    if (nresults > 0)
      return nresults;  /* results are already on the stack */
    else {
      lua_pushuserdata(L, NULL);  /* at least one result to signal no errors */
      return 1;
    }
  }
  else {  /* error */
    lua_pushnil(L);
    lua_pushnumber(L, status);  /* error code */
    return 2;
  }
}

int luaB_dostring (lua_State *L) {
  int oldtop = lua_gettop(L);
  size_t l;
  const char *s = luaL_check_lstr(L, 1, &l);
  if (*s == '\27')  /* binary files start with ESC... */
    lua_error(L, "`dostring' cannot run pre-compiled code");
  return passresults(L, lua_dobuffer(L, s, l, luaL_opt_string(L, 2, s)), oldtop);
}


int luaB_dofile (lua_State *L) {
  int oldtop = lua_gettop(L);
  const char *fname = luaL_opt_string(L, 1, NULL);
  return passresults(L, lua_dofile(L, fname), oldtop);
}

/* }====================================================== */


/*
** {======================================================
** Functions that could use only the official API but
** do not, for efficiency.
** =======================================================
*/

#include "lapi.h"
#include "ldo.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lvm.h"


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
  luaL_checktype(L, arg, "table");
  return hvalue(luaA_index(L, arg));
}

/* }====================================================== */




int luaB_call (lua_State *L) {
  int oldtop;
  const Hash *arg = gettable(L, 2);
  const char *options = luaL_opt_string(L, 3, "");
  int err = 0;  /* index of old error method */
  int n = (int)getnarg(L, arg);
  int i, status;
  if (!lua_isnull(L, 4)) {  /* set new error method */
    lua_getglobal(L, LUA_ERRORMESSAGE);
    err = lua_gettop(L);  /* get index */
    lua_pushobject(L, 4);
    lua_setglobal(L, LUA_ERRORMESSAGE);
  }
  oldtop = lua_gettop(L);  /* top before function-call preparation */
  /* push function */
  lua_pushobject(L, 1);
  /* push arg[1...n] */
  luaD_checkstack(L, n);
  for (i=0; i<n; i++)
    *(L->top++) = *luaH_getnum(arg, i+1);
  status = lua_call(L, n, LUA_MULTRET);
  if (err != 0) {  /* restore old error method */
    lua_pushobject(L, err);
    lua_setglobal(L, LUA_ERRORMESSAGE);
  }
  if (status != 0) {  /* error in call? */
    if (strchr(options, 'x'))
      lua_pushnil(L);  /* return nil to signal the error */
    else
      lua_error(L, NULL);  /* propagate error without additional messages */
    return 1;
  }
  else {  /* no errors */
    if (strchr(options, 'p')) {  /* pack results? */
      luaV_pack(L, luaA_index(L, oldtop+1));
      return 1;  /* only table is returned */
    }
    else
      return lua_gettop(L) - oldtop;  /* results are already on the stack */
  }
}



int luaB_tostring (lua_State *L) {
  char buff[64];
  const TObject *o;
  luaL_checktype(L, 1, "any");
  o = luaA_index(L, 1);
  switch (ttype(o)) {
    case TAG_NUMBER:
      lua_pushstring(L, lua_tostring(L, 1));
      return 1;
    case TAG_STRING:
      lua_pushobject(L, 1);
      return 1;
    case TAG_TABLE:
      sprintf(buff, "table: %p", hvalue(o));
      break;
    case TAG_LCLOSURE:  case TAG_CCLOSURE:
      sprintf(buff, "function: %p", clvalue(o));
      break;
    case TAG_USERDATA:
      sprintf(buff, "userdata(%d): %p", lua_tag(L, 1), lua_touserdata(L, 1));
      break;
    case TAG_NIL:
      lua_pushstring(L, "nil");
      return 1;
    default:
      LUA_INTERNALERROR("invalid type");
  }
  lua_pushstring(L, buff);
  return 1;
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

int luaB_assert (lua_State *L) {
  luaL_checktype(L, 1, "any");
  if (lua_isnil(L, 1))
    luaL_verror(L, "assertion failed!  %.90s", luaL_opt_string(L, 2, ""));
  return 0;
}


int luaB_getn (lua_State *L) {
  lua_pushnumber(L, getnarg(L, gettable(L, 1)));
  return 1;
}


/* auxiliary function */
static void t_move (lua_State *L, Hash *t, int from, int to) {
  TObject *p = luaH_setint(L, t, to);  /* may change following `get' */
  *p = *luaH_getnum(t, from);
}


int luaB_tinsert (lua_State *L) {
  Hash *a = gettable(L, 1);
  int n = (int)getnarg(L, a);
  int v = lua_gettop(L);  /* last argument: to be inserted */
  int pos;
  if (v == 2)  /* called with only 2 arguments */
    pos = n+1;
  else
    pos = luaL_check_int(L, 2);  /* 2nd argument is the position */
  luaH_setstrnum(L, a, luaS_new(L, "n"), n+1);  /* a.n = n+1 */
  for (; n>=pos; n--)
    t_move(L, a, n, n+1);  /* a[n+1] = a[n] */
  *luaH_setint(L, a, pos) = *luaA_index(L, v);  /* a[pos] = v */
  return 0;
}


int luaB_tremove (lua_State *L) {
  Hash *a = gettable(L, 1);
  int n = (int)getnarg(L, a);
  int pos = luaL_opt_int(L, 2, n);
  if (n <= 0) return 0;  /* table is "empty" */
  luaA_pushobject(L, luaH_getnum(a, pos));  /* result = a[pos] */
  for ( ;pos<n; pos++)
    t_move(L, a, pos+1, pos);  /* a[pos] = a[pos+1] */
  luaH_setstrnum(L, a, luaS_new(L, "n"), n-1);  /* a.n = n-1 */
  ttype(luaH_setint(L, a, n)) = TAG_NIL;  /* a[n] = nil */
  return 1;
}


static int luaB_foreachi (lua_State *L) {
  const Hash *t = gettable(L, 1);
  int n = (int)getnarg(L, t);
  int i;
  luaL_checktype(L, 2, "function");
  for (i=1; i<=n; i++) {
    lua_pushobject(L, 2);
    ttype(L->top) = TAG_NUMBER; nvalue(L->top++) = i;
    *(L->top++) = *luaH_getnum(t, i);
    luaD_call(L, L->top-3, 1);
    if (ttype(L->top-1) != TAG_NIL)
      return 1;
    L->top--;  /* remove nil result */
  }
  return 0;
}


static int luaB_foreach (lua_State *L) {
  luaL_checktype(L, 1, "table");
  luaL_checktype(L, 2, "function");
  lua_pushobject(L, 1);  /* put table at top */
  lua_pushnil(L);  /* first index */
  for (;;) {
    if (lua_next(L) == 0)
      return 0;
    lua_pushobject(L, 2);  /* function */
    lua_pushobject(L, -3);  /* key */
    lua_pushobject(L, -3);  /* value */
    if (lua_call(L, 2, 1) != 0) lua_error(L, NULL);
    if (!lua_isnil(L, -1))
      return 1;
    lua_pop(L, 2);  /* remove value and result */
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
  t_move(L, a, j, i);
  *luaH_setint(L, a, j) = temp;
}

static int sort_comp (lua_State *L, const TObject *f, const TObject *a,
                                                      const TObject *b) {
  /* WARNING: the caller (auxsort) must ensure stack space */
  if (f != NULL) {
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

static void auxsort (lua_State *L, Hash *a, int l, int u, const TObject *f) {
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

int luaB_sort (lua_State *L) {
  Hash *a = gettable(L, 1);
  int n = (int)getnarg(L, a);
  const TObject *func = NULL;
  if (!lua_isnull(L, 2)) {  /* is there a 2nd argument? */
    luaL_checktype(L, 2, "function");
    func = luaA_index(L, 2);
  }
  auxsort(L, a, 1, n, func);
  return 0;
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
  hvalue(&gt) = L->gt;
  for (i=0; i<num_deprecated; i++) {
    lua_pushobject(L, &gt);
    lua_pushcclosure(L, deprecated_global_funcs[i].func, 1); ??
    lua_setglobal(L, deprecated_global_funcs[i].name);
  }
  luaL_openl(L, other_deprecated_global_funcs);
}

#else

/*
** gives an explicit error in any attempt to call a deprecated function
*/
static int deprecated_func (lua_State *L) {
  luaL_verror(L, "function `%.20s' is deprecated", luaL_check_string(L, -1));
  return 0;  /* to avoid warnings */
}


#define num_deprecated	6

static const char *const deprecated_names [num_deprecated] = {
  "foreachvar", "nextvar", "rawgetglobal",
  "rawgettable", "rawsetglobal", "rawsettable"
};


static void deprecated_funcs (lua_State *L) {
  int i;
  for (i=0; i<num_deprecated; i++) {
    lua_pushstring(L, deprecated_names[i]);
    lua_pushcclosure(L, deprecated_func, 1);
    lua_setglobal(L, deprecated_names[i]);
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
  luaL_openl(L, builtin_funcs);
#ifdef DEBUG
  luaB_opentests(L);  /* internal test functions */
#endif
  lua_pushstring(L, LUA_VERSION);
  lua_setglobal(L, "_VERSION");
  deprecated_funcs(L);
}

