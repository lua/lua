/*
** $Id: lbaselib.c,v 1.27 2001/02/23 17:17:25 roberto Exp roberto $
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
    lua_pushliteral(L, l_s("error: "));
    lua_pushvalue(L, 1);
    if (lua_getstack(L, 1, &ar)) {
      lua_getinfo(L, l_s("Sl"), &ar);
      if (ar.source && ar.currentline > 0) {
        l_char buff[100];
        sprintf(buff, l_s("\n  <%.70s: line %d>"), ar.short_src, ar.currentline);
        lua_pushstring(L, buff);
        lua_concat(L, 2);
      }
    }
    lua_pushliteral(L, l_s("\n"));
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
  lua_getglobal(L, l_s("tostring"));
  for (i=1; i<=n; i++) {
    const l_char *s;
    lua_pushvalue(L, -1);  /* function to be called */
    lua_pushvalue(L, i);   /* value to print */
    lua_rawcall(L, 1, 1);
    s = lua_tostring(L, -1);  /* get result */
    if (s == NULL)
      lua_error(L, l_s("`tostring' must return a string to `print'"));
    if (i>1) fputs(l_s("\t"), stdout);
    fputs(s, stdout);
    lua_pop(L, 1);  /* pop result */
  }
  fputs(l_s("\n"), stdout);
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
    const l_char *s1 = luaL_check_string(L, 1);
    l_char *s2;
    unsigned long n;
    luaL_arg_check(L, 2 <= base && base <= 36, 2, l_s("base out of range"));
    n = strtoul(s1, &s2, base);
    if (s1 != s2) {  /* at least one valid digit? */
      while (isspace(uchar(*s2))) s2++;  /* skip trailing spaces */
      if (*s2 == l_c('\0')) {  /* no invalid trailing characters? */
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


/* auxiliary function to get `tags' */
static int gettag (lua_State *L, int narg) {
  switch (lua_type(L, narg)) {
    case LUA_TNUMBER:
      return (int)lua_tonumber(L, narg);
    case LUA_TSTRING: {
      const l_char *name = lua_tostring(L, narg);
      int tag = lua_type2tag(L, name);
      if (tag == LUA_TNONE)
        luaL_verror(L, l_s("'%.30s' is not a valid type name"), name);
      return tag;
    }
    default:
      luaL_argerror(L, narg, l_s("tag or type name expected"));
      return 0;  /* to avoid warnings */
  }
}


static int luaB_tag (lua_State *L) {
  luaL_checkany(L, 1);
  lua_pushnumber(L, lua_tag(L, 1));
  return 1;
}

static int luaB_settag (lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  lua_pushvalue(L, 1);  /* push table */
  lua_settag(L, gettag(L, 2));
  return 1;  /* return table */
}

static int luaB_newtype (lua_State *L) {
  const l_char *name = luaL_opt_string(L, 1, NULL);
  lua_pushnumber(L, lua_newtype(L, name, LUA_TTABLE));
  return 1;
}

static int luaB_copytagmethods (lua_State *L) {
  lua_pushnumber(L, lua_copytagmethods(L, gettag(L, 1), gettag(L, 2)));
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
  lua_rawget(L, -2);
  return 1;
}

static int luaB_rawset (lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  luaL_checkany(L, 2);
  luaL_checkany(L, 3);
  lua_rawset(L, -3);
  return 1;
}

static int luaB_settagmethod (lua_State *L) {
  int tag = gettag(L, 1);
  const l_char *event = luaL_check_string(L, 2);
  luaL_arg_check(L, lua_isfunction(L, 3) || lua_isnil(L, 3), 3,
                 l_s("function or nil expected"));
  if (strcmp(event, l_s("gc")) == 0)
    lua_error(L, l_s("deprecated use: cannot set the `gc' tag method from Lua"));
  lua_gettagmethod(L, tag, event);
  lua_pushvalue(L, 3);
  lua_settagmethod(L, tag, event);
  return 1;
}


static int luaB_gettagmethod (lua_State *L) {
  int tag = gettag(L, 1);
  const l_char *event = luaL_check_string(L, 2);
  if (strcmp(event, l_s("gc")) == 0)
    lua_error(L, l_s("deprecated use: cannot get the `gc' tag method from Lua"));
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


static int luaB_xtype (lua_State *L) {
  luaL_checkany(L, 1);
  lua_pushstring(L, lua_xtype(L, 1));
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
  static const l_char *const errornames[] =
    {l_s("ok"), l_s("run-time error"), l_s("file error"), l_s("syntax error"),
     l_s("memory error"), l_s("error in error handling")};
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
  const l_char *s = luaL_check_lstr(L, 1, &l);
  const l_char *chunkname = luaL_opt_string(L, 2, s);
  return passresults(L, lua_dobuffer(L, s, l, chunkname), oldtop);
}


static int luaB_dofile (lua_State *L) {
  int oldtop = lua_gettop(L);
  const l_char *fname = luaL_opt_string(L, 1, NULL);
  return passresults(L, lua_dofile(L, fname), oldtop);
}


static int luaB_call (lua_State *L) {
  int oldtop;
  const l_char *options = luaL_opt_string(L, 3, l_s(""));
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
  luaL_checkstack(L, n, l_s("too many arguments"));
  for (i=0; i<n; i++)  /* push arg[1...n] */
    lua_rawgeti(L, 2, i+1);
  status = lua_call(L, n, LUA_MULTRET);
  if (err != 0) {  /* restore old error method */
    lua_pushvalue(L, err);
    lua_setglobal(L, LUA_ERRORMESSAGE);
  }
  if (status != 0) {  /* error in call? */
    if (strchr(options, l_c('x')))
      lua_pushnil(L);  /* return nil to signal the error */
    else
      lua_error(L, NULL);  /* propagate error without additional messages */
    return 1;
  }
  if (strchr(options, l_c('p')))  /* pack results? */
    lua_error(L, l_s("deprecated option `p' in `call'"));
  return lua_gettop(L) - oldtop;  /* results are already on the stack */
}


static int luaB_tostring (lua_State *L) {
  l_char buff[64];
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER:
      lua_pushstring(L, lua_tostring(L, 1));
      return 1;
    case LUA_TSTRING:
      lua_pushvalue(L, 1);
      return 1;
    case LUA_TTABLE:
      sprintf(buff, l_s("%.40s: %p"), lua_xtype(L, 1), lua_topointer(L, 1));
      break;
    case LUA_TFUNCTION:
      sprintf(buff, l_s("function: %p"), lua_topointer(L, 1));
      break;
    case LUA_TUSERDATA: {
      const l_char *t = lua_xtype(L, 1);
      if (strcmp(t, l_s("userdata")) == 0)
        sprintf(buff, l_s("userdata(%d): %p"), lua_tag(L, 1), lua_touserdata(L, 1));
      else
        sprintf(buff, l_s("%.40s: %p"), t, lua_touserdata(L, 1));
      break;
    }
    case LUA_TNIL:
      lua_pushliteral(L, l_s("nil"));
      return 1;
    default:
      luaL_argerror(L, 1, l_s("value expected"));
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
    luaL_verror(L, l_s("assertion failed!  %.90s"), luaL_opt_string(L, 2, l_s("")));
  lua_settop(L, 1);
  return 1;
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
  lua_pushliteral(L, l_s("n"));
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
  if (n <= 0) return 0;  /* table is `empty' */
  lua_rawgeti(L, 1, pos);  /* result = t[pos] */
  for ( ;pos<n; pos++) {
    lua_rawgeti(L, 1, pos+1);
    lua_rawseti(L, 1, pos);  /* a[pos] = a[pos+1] */
  }
  lua_pushliteral(L, l_s("n"));
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
        if (i>u) lua_error(L, l_s("invalid order function for sorting"));
        lua_pop(L, 1);  /* remove a[i] */
      }
      /* repeat --j until a[j] <= P */
      while (lua_rawgeti(L, 1, --j), sort_comp(L, -3, -1)) {
        if (j<l) lua_error(L, l_s("invalid order function for sorting"));
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
    /* adjust so that smaller half is in [j..i] and larger one in [l..u] */
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

static const luaL_reg deprecated_names [num_deprecated] = {
  {l_s("foreachvar"), luaB_foreach},
  {l_s("nextvar"), luaB_next},
  {l_s("rawgetglobal"), luaB_rawget},
  {l_s("rawsetglobal"), luaB_rawset}
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
  luaL_verror(L, l_s("function `%.20s' is deprecated"), lua_tostring(L, -1));
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

static const luaL_reg base_funcs[] = {
  {LUA_ALERT, luaB__ALERT},
  {LUA_ERRORMESSAGE, luaB__ERRORMESSAGE},
  {l_s("call"), luaB_call},
  {l_s("collectgarbage"), luaB_collectgarbage},
  {l_s("copytagmethods"), luaB_copytagmethods},
  {l_s("dofile"), luaB_dofile},
  {l_s("dostring"), luaB_dostring},
  {l_s("error"), luaB_error},
  {l_s("foreach"), luaB_foreach},
  {l_s("foreachi"), luaB_foreachi},
  {l_s("gcinfo"), luaB_gcinfo},
  {l_s("getglobal"), luaB_getglobal},
  {l_s("gettagmethod"), luaB_gettagmethod},
  {l_s("globals"), luaB_globals},
  {l_s("newtype"), luaB_newtype},
  {l_s("newtag"), luaB_newtype},  /* for compatibility 4.0 */
  {l_s("next"), luaB_next},
  {l_s("print"), luaB_print},
  {l_s("rawget"), luaB_rawget},
  {l_s("rawset"), luaB_rawset},
  {l_s("rawgettable"), luaB_rawget},  /* for compatibility 3.2 */
  {l_s("rawsettable"), luaB_rawset},  /* for compatibility 3.2 */
  {l_s("setglobal"), luaB_setglobal},
  {l_s("settag"), luaB_settag},
  {l_s("settagmethod"), luaB_settagmethod},
  {l_s("tag"), luaB_tag},
  {l_s("tonumber"), luaB_tonumber},
  {l_s("tostring"), luaB_tostring},
  {l_s("type"), luaB_type},
  {l_s("assert"), luaB_assert},
  {l_s("getn"), luaB_getn},
  {l_s("sort"), luaB_sort},
  {l_s("tinsert"), luaB_tinsert},
  {l_s("tremove"), luaB_tremove},
  {l_s("xtype"), luaB_xtype},
};



LUALIB_API void lua_baselibopen (lua_State *L) {
  luaL_openl(L, base_funcs);
  lua_pushliteral(L, LUA_VERSION);
  lua_setglobal(L, l_s("_VERSION"));
  deprecated_funcs(L);
}

