/*
** $Id: ldblib.c,v 1.8 1999/11/22 17:39:51 roberto Exp roberto $
** Interface from Lua to its debug API
** See Copyright Notice in lua.h
*/


#include <stdlib.h>
#include <string.h>

#define LUA_REENTRANT

#include "lauxlib.h"
#include "lua.h"
#include "luadebug.h"
#include "lualib.h"



static void settabss (lua_State *L, lua_Object t, const char *i, const char *v) {
  lua_pushobject(L, t);
  lua_pushstring(L, i);
  lua_pushstring(L, v);
  lua_settable(L);
}


static void settabsi (lua_State *L, lua_Object t, const char *i, int v) {
  lua_pushobject(L, t);
  lua_pushstring(L, i);
  lua_pushnumber(L, v);
  lua_settable(L);
}


static lua_Object getfuncinfo (lua_State *L, lua_Object func) {
  lua_Object result = lua_createtable(L);
  const char *str;
  int line;
  lua_funcinfo(L, func, &str, &line);
  if (line == -1)  /* C function? */
    settabss(L, result, "kind", "C");
  else if (line == 0) {  /* "main"? */
      settabss(L, result, "kind", "chunk");
      settabss(L, result, "source", str);
    }
  else {  /* Lua function */
    settabss(L, result, "kind", "Lua");
    settabsi(L, result, "def_line", line);
    settabss(L, result, "source", str);
  }
  if (line != 0) {  /* is it not a "main"? */
    const char *kind = lua_getobjname(L, func, &str);
    if (*kind) {
      settabss(L, result, "name", str);
      settabss(L, result, "where", kind);
    }
  }
  return result;
}


static void getstack (lua_State *L) {
  lua_Object func = lua_stackedfunction(L, luaL_check_int(L, 1));
  if (func == LUA_NOOBJECT)  /* level out of range? */
    return;
  else {
    lua_Object result = getfuncinfo(L, func);
    int currline = lua_currentline(L, func);
    if (currline > 0)
      settabsi(L, result, "current", currline);
    lua_pushobject(L, result);
    lua_pushstring(L, "func");
    lua_pushobject(L, func);
    lua_settable(L);  /* result.func = func */
    lua_pushobject(L, result);
  }
}


static void funcinfo (lua_State *L) {
  lua_pushobject(L, getfuncinfo(L, luaL_functionarg(L, 1)));
}


static int findlocal (lua_State *L, lua_Object func, int arg) {
  lua_Object v = lua_getparam(L, arg);
  if (lua_isnumber(L, v))
    return (int)lua_getnumber(L, v);
  else {
    const char *name = luaL_check_string(L, arg);
    int i = 0;
    int result = -1;
    const char *vname;
    while (lua_getlocal(L, func, ++i, &vname) != LUA_NOOBJECT) {
      if (strcmp(name, vname) == 0)
        result = i;  /* keep looping to get the last var with this name */
    }
    if (result == -1)
      luaL_verror(L, "no local variable `%.50s' at given level", name);
    return result;
  }
}


static void getlocal (lua_State *L) {
  lua_Object func = lua_stackedfunction(L, luaL_check_int(L, 1));
  lua_Object val;
  const char *name;
  if (func == LUA_NOOBJECT)  /* level out of range? */
    return;  /* return nil */
  else if (lua_getparam(L, 2) != LUA_NOOBJECT) {  /* 2nd argument? */
    if ((val = lua_getlocal(L, func, findlocal(L, func, 2), &name)) != LUA_NOOBJECT) {
      lua_pushobject(L, val);
      lua_pushstring(L, name);
    }
    /* else return nil */
  }
  else {  /* collect all locals in a table */
    lua_Object result = lua_createtable(L);
    int i;
    for (i=1; ;i++) {
      if ((val = lua_getlocal(L, func, i, &name)) == LUA_NOOBJECT)
        break;
      lua_pushobject(L, result);
      lua_pushstring(L, name);
      lua_pushobject(L, val);
      lua_settable(L);  /* result[name] = value */
    }
    lua_pushobject(L, result);
  }
}


static void setlocal (lua_State *L) {
  lua_Object func = lua_stackedfunction(L, luaL_check_int(L, 1));
  int numvar;
  luaL_arg_check(L, func != LUA_NOOBJECT, 1, "level out of range");
  numvar = findlocal(L, func, 2);
  lua_pushobject(L, luaL_nonnullarg(L, 3));
  if (!lua_setlocal(L, func, numvar))
    lua_error(L, "no such local variable");
}



static int linehook = LUA_NOREF;  /* Lua reference to line hook function */
static int callhook = LUA_NOREF;  /* Lua reference to call hook function */



static void linef (lua_State *L, int line) {
  if (linehook != LUA_NOREF) {
    lua_pushnumber(L, line);
    lua_callfunction(L, lua_getref(L, linehook));
  }
}


static void callf (lua_State *L, lua_Function f, const char *file, int line) {
  if (callhook != LUA_NOREF) {
    if (f != LUA_NOOBJECT) {
      lua_pushobject(L, f);
      lua_pushstring(L, file);
      lua_pushnumber(L, line);
    }
    lua_callfunction(L, lua_getref(L, callhook));
  }
}


static void setcallhook (lua_State *L) {
  lua_Object f = lua_getparam(L, 1);
  lua_unref(L, callhook);
  if (f == LUA_NOOBJECT) {
    callhook = LUA_NOREF;
    lua_setcallhook(L, NULL);
  }
  else {
    lua_pushobject(L, f);
    callhook = lua_ref(L, 1);
    lua_setcallhook(L, callf);
  }
}


static void setlinehook (lua_State *L) {
  lua_Object f = lua_getparam(L, 1);
  lua_unref(L, linehook);
  if (f == LUA_NOOBJECT) {
    linehook = LUA_NOREF;
    lua_setlinehook(L, NULL);
  }
  else {
    lua_pushobject(L, f);
    linehook = lua_ref(L, 1);
    lua_setlinehook(L, linef);
  }
}


static const struct luaL_reg dblib[] = {
  {"funcinfo", funcinfo},
  {"getlocal", getlocal},
  {"getstack", getstack},
  {"setcallhook", setcallhook},
  {"setlinehook", setlinehook},
  {"setlocal", setlocal}
};


void lua_dblibopen (lua_State *L) {
  luaL_openl(L, dblib);
}

