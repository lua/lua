/*
** $Id: lbaselib.c,v 1.17a 2000/11/06 13:45:18 roberto Exp $
** Basic library
** See Copyright Notice in lua.h
*/



#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"

#include "lauxlib.h"
#include "luadebug.h"
#include "lualib.h"



/*
** If your system does not support `stderr', redefine this function, or
** redefine _ERRORMESSAGE so that it won't need _ALERT.
*/
static int luaB__ALERT (lua_State *L) {
  fputs(luaL_check_string(L, 1), stderr);
  return 0;
}


/*
** Basic implementation of _ERRORMESSAGE.
** The library `liolib' redefines _ERRORMESSAGE for better error information.
*/
static int luaB__ERRORMESSAGE (lua_State *L) {
  luaL_checktype(L, 1, LUA_TSTRING);
  lua_getglobal(L, LUA_ALERT);
  if (lua_isfunction(L, -1)) {  /* avoid error loop if _ALERT is not defined */
    lua_Debug ar;
    lua_pushstring(L, "error: ");
    lua_pushvalue(L, 1);
    if (lua_getstack(L, 1, &ar)) {
      lua_getinfo(L, "Sl", &ar);
      if (ar.source && ar.currentline > 0) {
        char buff[100];
        sprintf(buff, "\n  <%.70s: line %d>", ar.short_src, ar.currentline);
        lua_pushstring(L, buff);
        lua_concat(L, 2);
      }
    }
    lua_pushstring(L, "\n");
    lua_concat(L, 3);
    lua_rawcall(L, 1, 0);
  }
  return 0;
}


/*
** If your system does not support `stdout', you can just remove this function.
** If you need, you can define your own `print' function, following this
** model but changing `fputs' to put the strings at a proper place
** (a console window or a log file, for instance).
*/
static int luaB_print (lua_State *L) {
  int n = lua_gettop(L);  /* number of arguments */
  int i;
  lua_getglobal(L, "tostring");
  for (i=1; i<=n; i++) {
    const char *s;
    lua_pushvalue(L, -1);  /* function to be called */
    lua_pushvalue(L, i);   /* value to print */
    lua_rawcall(L, 1, 1);
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


static int luaB_tonumber (lua_State *L) {
  int base = luaL_opt_int(L, 2, 10);
  if (base == 10) {  /* standard conversion */
    luaL_checkany(L, 1);
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


static int luaB_error (lua_State *L) {
  lua_error(L, luaL_opt_string(L, 1, NULL));
  return 0;  /* to avoid warnings */
}

static int luaB_setglobal (lua_State *L) {
  luaL_checkany(L, 2);
  lua_setglobal(L, luaL_check_string(L, 1));
  return 0;
}

static int luaB_getglobal (lua_State *L) {
  lua_getglobal(L, luaL_check_string(L, 1));
  return 1;
}

static int luaB_tag (lua_State *L) {
  luaL_checkany(L, 1);
  lua_pushnumber(L, lua_tag(L, 1));
  return 1;
}

static int luaB_settag (lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  lua_pushvalue(L, 1);  /* push table */
  lua_settag(L, luaL_check_int(L, 2));
  return 1;  /* return table */
}

static int luaB_newtag (lua_State *L) {
  lua_pushnumber(L, lua_newtag(L));
  return 1;
}

static int luaB_copytagmethods (lua_State *L) {
  lua_pushnumber(L, lua_copytagmethods(L, luaL_check_int(L, 1),
                                          luaL_check_int(L, 2)));
  return 1;
}

static int luaB_globals (lua_State *L) {
  lua_getglobals(L);  /* value to be returned */
  if (!lua_isnull(L, 1)) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_pushvalue(L, 1);  /* new table of globals */
    lua_setglobals(L);
  }
  return 1;
}

static int luaB_rawget (lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  luaL_checkany(L, 2);
  lua_rawget(L, 1);
  return 1;
}

static int luaB_rawset (lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  luaL_checkany(L, 2);
  luaL_checkany(L, 3);
  lua_rawset(L, 1);
  return 1;
}

static int luaB_settagmethod (lua_State *L) {
  int tag = luaL_check_int(L, 1);
  const char *event = luaL_check_string(L, 2);
  luaL_arg_check(L, lua_isfunction(L, 3) || lua_isnil(L, 3), 3,
                 "function or nil expected");
  if (strcmp(event, "gc") == 0)
    lua_error(L, "deprecated use: cannot set the `gc' tag method from Lua");
  lua_gettagmethod(L, tag, event);
  lua_pushvalue(L, 3);
  lua_settagmethod(L, tag, event);
  return 1;
}


static int luaB_gettagmethod (lua_State *L) {
  int tag = luaL_check_int(L, 1);
  const char *event = luaL_check_string(L, 2);
  if (strcmp(event, "gc") == 0)
    lua_error(L, "deprecated use: cannot get the `gc' tag method from Lua");
  lua_gettagmethod(L, tag, event);
  return 1;
}


static int luaB_gcinfo (lua_State *L) {
  lua_pushnumber(L, lua_getgccount(L));
  lua_pushnumber(L, lua_getgcthreshold(L));
  return 2;
}


static int luaB_collectgarbage (lua_State *L) {
  lua_setgcthreshold(L, luaL_opt_int(L, 1, 0));
  return 0;
}


static int luaB_type (lua_State *L) {
  luaL_checkany(L, 1);
  lua_pushstring(L, lua_typename(L, lua_type(L, 1)));
  return 1;
}


static int luaB_next (lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  lua_settop(L, 2);  /* create a 2nd argument if there isn't one */
  if (lua_next(L, 1))
    return 2;
  else {
    lua_pushnil(L);
    return 1;
  }
}


static int passresults (lua_State *L, int status, int oldtop) {
  static const char *const errornames[] =
    {"ok", "run-time error", "file error", "syntax error",
     "memory error", "error in error handling"};
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
    lua_pushstring(L, errornames[status]);  /* error code */
    return 2;
  }
}

static int luaB_dostring (lua_State *L) {
  int oldtop = lua_gettop(L);
  size_t l;
  const char *s = luaL_check_lstr(L, 1, &l);
  if (*s == '\33')  /* binary files start with ESC... */
    lua_error(L, "`dostring' cannot run pre-compiled code");
  return passresults(L, lua_dobuffer(L, s, l, luaL_opt_string(L, 2, s)), oldtop);
}


static int luaB_dofile (lua_State *L) {
  int oldtop = lua_gettop(L);
  const char *fname = luaL_opt_string(L, 1, NULL);
  return passresults(L, lua_dofile(L, fname), oldtop);
}


static int luaB_call (lua_State *L) {
  int oldtop;
  const char *options = luaL_opt_string(L, 3, "");
  int err = 0;  /* index of old error method */
  int i, status;
  int n;
  luaL_checktype(L, 2, LUA_TTABLE);
  n = lua_getn(L, 2);
  if (!lua_isnull(L, 4)) {  /* set new error method */
    lua_getglobal(L, LUA_ERRORMESSAGE);
    err = lua_gettop(L);  /* get index */
    lua_pushvalue(L, 4);
    lua_setglobal(L, LUA_ERRORMESSAGE);
  }
  oldtop = lua_gettop(L);  /* top before function-call preparation */
  /* push function */
  lua_pushvalue(L, 1);
  luaL_checkstack(L, n, "too many arguments");
  for (i=0; i<n; i++)  /* push arg[1...n] */
    lua_rawgeti(L, 2, i+1);
  status = lua_call(L, n, LUA_MULTRET);
  if (err != 0) {  /* restore old error method */
    lua_pushvalue(L, err);
    lua_setglobal(L, LUA_ERRORMESSAGE);
  }
  if (status != 0) {  /* error in call? */
    if (strchr(options, 'x'))
      lua_pushnil(L);  /* return nil to signal the error */
    else
      lua_error(L, NULL);  /* propagate error without additional messages */
    return 1;
  }
  if (strchr(options, 'p'))  /* pack results? */
    lua_error(L, "deprecated option `p' in `call'");
  return lua_gettop(L) - oldtop;  /* results are already on the stack */
}


static int luaB_tostring (lua_State *L) {
  char buff[64];
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER:
      lua_pushstring(L, lua_tostring(L, 1));
      return 1;
    case LUA_TSTRING:
      lua_pushvalue(L, 1);
      return 1;
    case LUA_TTABLE:
      sprintf(buff, "table: %p", lua_topointer(L, 1));
      break;
    case LUA_TFUNCTION:
      sprintf(buff, "function: %p", lua_topointer(L, 1));
      break;
    case LUA_TUSERDATA:
      sprintf(buff, "userdata(%d): %p", lua_tag(L, 1), lua_touserdata(L, 1));
      break;
    case LUA_TNIL:
      lua_pushstring(L, "nil");
      return 1;
    default:
      luaL_argerror(L, 1, "value expected");
  }
  lua_pushstring(L, buff);
  return 1;
}


static int luaB_foreachi (lua_State *L) {
  int n, i;
  luaL_checktype(L, 1, LUA_TTABLE);
  luaL_checktype(L, 2, LUA_TFUNCTION);
  n = lua_getn(L, 1);
  for (i=1; i<=n; i++) {
    lua_pushvalue(L, 2);  /* function */
    lua_pushnumber(L, i);  /* 1st argument */
    lua_rawgeti(L, 1, i);  /* 2nd argument */
    lua_rawcall(L, 2, 1);
    if (!lua_isnil(L, -1))
      return 1;
    lua_pop(L, 1);  /* remove nil result */
  }
  return 0;
}


static int luaB_foreach (lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  luaL_checktype(L, 2, LUA_TFUNCTION);
  lua_pushnil(L);  /* first index */
  for (;;) {
    if (lua_next(L, 1) == 0)
      return 0;
    lua_pushvalue(L, 2);  /* function */
    lua_pushvalue(L, -3);  /* key */
    lua_pushvalue(L, -3);  /* value */
    lua_rawcall(L, 2, 1);
    if (!lua_isnil(L, -1))
      return 1;
    lua_pop(L, 2);  /* remove value and result */
  }
}


static int luaB_assert (lua_State *L) {
  luaL_checkany(L, 1);
  if (lua_isnil(L, 1))
    luaL_verror(L, "assertion failed!  %.90s", luaL_opt_string(L, 2, ""));
  return 0;
}


static int luaB_getn (lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  lua_pushnumber(L, lua_getn(L, 1));
  return 1;
}


static int luaB_tinsert (lua_State *L) {
  int v = lua_gettop(L);  /* last argument: to be inserted */
  int n, pos;
  luaL_checktype(L, 1, LUA_TTABLE);
  n = lua_getn(L, 1);
  if (v == 2)  /* called with only 2 arguments */
    pos = n+1;
  else
    pos = luaL_check_int(L, 2);  /* 2nd argument is the position */
  lua_pushstring(L, "n");
  lua_pushnumber(L, n+1);
  lua_rawset(L, 1);  /* t.n = n+1 */
  for (; n>=pos; n--) {
    lua_rawgeti(L, 1, n);
    lua_rawseti(L, 1, n+1);  /* t[n+1] = t[n] */
  }
  lua_pushvalue(L, v);
  lua_rawseti(L, 1, pos);  /* t[pos] = v */
  return 0;
}


static int luaB_tremove (lua_State *L) {
  int pos, n;
  luaL_checktype(L, 1, LUA_TTABLE);
  n = lua_getn(L, 1);
  pos = luaL_opt_int(L, 2, n);
  if (n <= 0) return 0;  /* table is "empty" */
  lua_rawgeti(L, 1, pos);  /* result = t[pos] */
  for ( ;pos<n; pos++) {
    lua_rawgeti(L, 1, pos+1);
    lua_rawseti(L, 1, pos);  /* a[pos] = a[pos+1] */
  }
  lua_pushstring(L, "n");
  lua_pushnumber(L, n-1);
  lua_rawset(L, 1);  /* t.n = n-1 */
  lua_pushnil(L);
  lua_rawseti(L, 1, n);  /* t[n] = nil */
  return 1;
}




/*
** {======================================================
** Quicksort
** (based on `Algorithms in MODULA-3', Robert Sedgewick;
**  Addison-Wesley, 1993.)
*/


static void set2 (lua_State *L, int i, int j) {
  lua_rawseti(L, 1, i);
  lua_rawseti(L, 1, j);
}

static int sort_comp (lua_State *L, int a, int b) {
  /* WARNING: the caller (auxsort) must ensure stack space */
  if (!lua_isnil(L, 2)) {  /* function? */
    int res;
    lua_pushvalue(L, 2);
    lua_pushvalue(L, a-1);  /* -1 to compensate function */
    lua_pushvalue(L, b-2);  /* -2 to compensate function and `a' */
    lua_rawcall(L, 2, 1);
    res = !lua_isnil(L, -1);
    lua_pop(L, 1);
    return res;
  }
  else  /* a < b? */
    return lua_lessthan(L, a, b);
}

static void auxsort (lua_State *L, int l, int u) {
  while (l < u) {  /* for tail recursion */
    int i, j;
    /* sort elements a[l], a[(l+u)/2] and a[u] */
    lua_rawgeti(L, 1, l);
    lua_rawgeti(L, 1, u);
    if (sort_comp(L, -1, -2))  /* a[u] < a[l]? */
      set2(L, l, u);  /* swap a[l] - a[u] */
    else
      lua_pop(L, 2);
    if (u-l == 1) break;  /* only 2 elements */
    i = (l+u)/2;
    lua_rawgeti(L, 1, i);
    lua_rawgeti(L, 1, l);
    if (sort_comp(L, -2, -1))  /* a[i]<a[l]? */
      set2(L, i, l);
    else {
      lua_pop(L, 1);  /* remove a[l] */
      lua_rawgeti(L, 1, u);
      if (sort_comp(L, -1, -2))  /* a[u]<a[i]? */
        set2(L, i, u);
      else
        lua_pop(L, 2);
    }
    if (u-l == 2) break;  /* only 3 elements */
    lua_rawgeti(L, 1, i);  /* Pivot */
    lua_pushvalue(L, -1);
    lua_rawgeti(L, 1, u-1);
    set2(L, i, u-1);
    /* a[l] <= P == a[u-1] <= a[u], only need to sort from l+1 to u-2 */
    i = l; j = u-1;
    for (;;) {  /* invariant: a[l..i] <= P <= a[j..u] */
      /* repeat ++i until a[i] >= P */
      while (lua_rawgeti(L, 1, ++i), sort_comp(L, -1, -2)) {
        if (i>u) lua_error(L, "invalid order function for sorting");
        lua_pop(L, 1);  /* remove a[i] */
      }
      /* repeat --j until a[j] <= P */
      while (lua_rawgeti(L, 1, --j), sort_comp(L, -3, -1)) {
        if (j<l) lua_error(L, "invalid order function for sorting");
        lua_pop(L, 1);  /* remove a[j] */
      }
      if (j<i) {
        lua_pop(L, 3);  /* pop pivot, a[i], a[j] */
        break;
      }
      set2(L, i, j);
    }
    lua_rawgeti(L, 1, u-1);
    lua_rawgeti(L, 1, i);
    set2(L, u-1, i);  /* swap pivot (a[u-1]) with a[i] */
    /* a[l..i-1] <= a[i] == P <= a[i+1..u] */
    /* adjust so that smaller "half" is in [j..i] and larger one in [l..u] */
    if (i-l < u-i) {
      j=l; i=i-1; l=i+2;
    }
    else {
      j=i+1; i=u; u=j-2;
    }
    auxsort(L, j, i);  /* call recursively the smaller one */
  }  /* repeat the routine for the larger one */
}

static int luaB_sort (lua_State *L) {
  int n;
  luaL_checktype(L, 1, LUA_TTABLE);
  n = lua_getn(L, 1);
  if (!lua_isnull(L, 2))  /* is there a 2nd argument? */
    luaL_checktype(L, 2, LUA_TFUNCTION);
  lua_settop(L, 2);  /* make sure there is two arguments */
  auxsort(L, 1, n);
  return 0;
}

/* }====================================================== */



/*
** {======================================================
** Deprecated functions to manipulate global environment.
** =======================================================
*/


#define num_deprecated	4

static const struct luaL_reg deprecated_names [num_deprecated] = {
  {"foreachvar", luaB_foreach},
  {"nextvar", luaB_next},
  {"rawgetglobal", luaB_rawget},
  {"rawsetglobal", luaB_rawset}
};


#ifdef LUA_DEPRECATEDFUNCS

/*
** call corresponding function inserting `globals' as first argument
*/
static int deprecated_func (lua_State *L) {
  lua_insert(L, 1);  /* upvalue is the function to be called */
  lua_getglobals(L);
  lua_insert(L, 2);  /* table of globals is 1o argument */
  lua_rawcall(L, lua_gettop(L)-1, LUA_MULTRET);
  return lua_gettop(L);  /* return all results */
}


static void deprecated_funcs (lua_State *L) {
  int i;
  for (i=0; i<num_deprecated; i++) {
    lua_pushcfunction(L, deprecated_names[i].func);
    lua_pushcclosure(L, deprecated_func, 1);
    lua_setglobal(L, deprecated_names[i].name);
  }
}


#else

/*
** gives an explicit error in any attempt to call a deprecated function
*/
static int deprecated_func (lua_State *L) {
  luaL_verror(L, "function `%.20s' is deprecated", lua_tostring(L, -1));
  return 0;  /* to avoid warnings */
}


static void deprecated_funcs (lua_State *L) {
  int i;
  for (i=0; i<num_deprecated; i++) {
    lua_pushstring(L, deprecated_names[i].name);
    lua_pushcclosure(L, deprecated_func, 1);
    lua_setglobal(L, deprecated_names[i].name);
  }
}

#endif

/* }====================================================== */

static const struct luaL_reg base_funcs[] = {
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
  {"gcinfo", luaB_gcinfo},
  {"getglobal", luaB_getglobal},
  {"gettagmethod", luaB_gettagmethod},
  {"globals", luaB_globals},
  {"newtag", luaB_newtag},
  {"next", luaB_next},
  {"print", luaB_print},
  {"rawget", luaB_rawget},
  {"rawset", luaB_rawset},
  {"rawgettable", luaB_rawget},  /* for compatibility */
  {"rawsettable", luaB_rawset},  /* for compatibility */
  {"setglobal", luaB_setglobal},
  {"settag", luaB_settag},
  {"settagmethod", luaB_settagmethod},
  {"tag", luaB_tag},
  {"tonumber", luaB_tonumber},
  {"tostring", luaB_tostring},
  {"type", luaB_type},
  {"assert", luaB_assert},
  {"getn", luaB_getn},
  {"sort", luaB_sort},
  {"tinsert", luaB_tinsert},
  {"tremove", luaB_tremove}
};



LUALIB_API void lua_baselibopen (lua_State *L) {
  luaL_openl(L, base_funcs);
  lua_pushstring(L, LUA_VERSION);
  lua_setglobal(L, "_VERSION");
  deprecated_funcs(L);
}

