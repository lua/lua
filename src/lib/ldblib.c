/*
** $Id: ldblib.c,v 1.5 1999/03/04 21:17:26 roberto Exp $
** Interface from Lua to its debug API
** See Copyright Notice in lua.h
*/


#include <stdlib.h>
#include <string.h>

#include "lauxlib.h"
#include "lua.h"
#include "luadebug.h"
#include "lualib.h"



static void settabss (lua_Object t, char *i, char *v) {
  lua_pushobject(t);
  lua_pushstring(i);
  lua_pushstring(v);
  lua_settable();
}


static void settabsi (lua_Object t, char *i, int v) {
  lua_pushobject(t);
  lua_pushstring(i);
  lua_pushnumber(v);
  lua_settable();
}


static lua_Object getfuncinfo (lua_Object func) {
  lua_Object result = lua_createtable();
  char *str;
  int line;
  lua_funcinfo(func, &str, &line);
  if (line == -1)  /* C function? */
    settabss(result, "kind", "C");
  else if (line == 0) {  /* "main"? */
      settabss(result, "kind", "chunk");
      settabss(result, "source", str);
    }
  else {  /* Lua function */
    settabss(result, "kind", "Lua");
    settabsi(result, "def_line", line);
    settabss(result, "source", str);
  }
  if (line != 0) {  /* is it not a "main"? */
    char *kind = lua_getobjname(func, &str);
    if (*kind) {
      settabss(result, "name", str);
      settabss(result, "where", kind);
    }
  }
  return result;
}


static void getstack (void) {
  lua_Object func = lua_stackedfunction(luaL_check_int(1));
  if (func == LUA_NOOBJECT)  /* level out of range? */
    return;
  else {
    lua_Object result = getfuncinfo(func);
    int currline = lua_currentline(func);
    if (currline > 0)
      settabsi(result, "current", currline);
    lua_pushobject(result);
    lua_pushstring("func");
    lua_pushobject(func);
    lua_settable();  /* result.func = func */
    lua_pushobject(result);
  }
}


static void funcinfo (void) {
  lua_pushobject(getfuncinfo(luaL_functionarg(1)));
}


static int findlocal (lua_Object func, int arg) {
  lua_Object v = lua_getparam(arg);
  if (lua_isnumber(v))
    return (int)lua_getnumber(v);
  else {
    char *name = luaL_check_string(arg);
    int i = 0;
    int result = -1;
    char *vname;
    while (lua_getlocal(func, ++i, &vname) != LUA_NOOBJECT) {
      if (strcmp(name, vname) == 0)
        result = i;  /* keep looping to get the last var with this name */
    }
    if (result == -1)
      luaL_verror("no local variable `%.50s' at given level", name);
    return result;
  }
}


static void getlocal (void) {
  lua_Object func = lua_stackedfunction(luaL_check_int(1));
  lua_Object val;
  char *name;
  if (func == LUA_NOOBJECT)  /* level out of range? */
    return;  /* return nil */
  else if (lua_getparam(2) != LUA_NOOBJECT) {  /* 2nd argument? */
    if ((val = lua_getlocal(func, findlocal(func, 2), &name)) != LUA_NOOBJECT) {
      lua_pushobject(val);
      lua_pushstring(name);
    }
    /* else return nil */
  }
  else {  /* collect all locals in a table */
    lua_Object result = lua_createtable();
    int i;
    for (i=1; ;i++) {
      if ((val = lua_getlocal(func, i, &name)) == LUA_NOOBJECT)
        break;
      lua_pushobject(result);
      lua_pushstring(name);
      lua_pushobject(val);
      lua_settable();  /* result[name] = value */
    }
    lua_pushobject(result);
  }
}


static void setlocal (void) {
  lua_Object func = lua_stackedfunction(luaL_check_int(1));
  int numvar;
  luaL_arg_check(func != LUA_NOOBJECT, 1, "level out of range");
  numvar = findlocal(func, 2);
  lua_pushobject(luaL_nonnullarg(3));
  if (!lua_setlocal(func, numvar))
    lua_error("no such local variable");
}



static int linehook = -1;  /* Lua reference to line hook function */
static int callhook = -1;  /* Lua reference to call hook function */


static void dohook (int ref) {
  lua_LHFunction oldlinehook = lua_setlinehook(NULL);
  lua_CHFunction oldcallhook = lua_setcallhook(NULL);
  lua_callfunction(lua_getref(ref));
  lua_setlinehook(oldlinehook);
  lua_setcallhook(oldcallhook);
}


static void linef (int line) {
  lua_pushnumber(line);
  dohook(linehook);
}


static void callf (lua_Function func, char *file, int line) {
  if (func != LUA_NOOBJECT) {
    lua_pushobject(func);
    lua_pushstring(file);
    lua_pushnumber(line);
  }
  dohook(callhook);
}


static void setcallhook (void) {
  lua_Object f = lua_getparam(1);
  lua_unref(callhook);
  if (f == LUA_NOOBJECT) {
    callhook = -1;
    lua_setcallhook(NULL);
  }
  else {
    lua_pushobject(f);
    callhook = lua_ref(1);
    lua_setcallhook(callf);
  }
}


static void setlinehook (void) {
  lua_Object f = lua_getparam(1);
  lua_unref(linehook);
  if (f == LUA_NOOBJECT) {
    linehook = -1;
    lua_setlinehook(NULL);
  }
  else {
    lua_pushobject(f);
    linehook = lua_ref(1);
    lua_setlinehook(linef);
  }
}


static struct luaL_reg dblib[] = {
  {"funcinfo", funcinfo},
  {"getlocal", getlocal},
  {"getstack", getstack},
  {"setcallhook", setcallhook},
  {"setlinehook", setlinehook},
  {"setlocal", setlocal}
};


void lua_dblibopen (void) {
  luaL_openlib(dblib, (sizeof(dblib)/sizeof(dblib[0])));
}

