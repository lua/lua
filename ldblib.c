/*
** $Id: ldblib.c,v 1.17 2000/06/12 13:52:05 roberto Exp roberto $
** Interface from Lua to its debug API
** See Copyright Notice in lua.h
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"

#include "lauxlib.h"
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


static void settabso (lua_State *L, lua_Object t, const char *i, lua_Object v) {
  lua_pushobject(L, t);
  lua_pushstring(L, i);
  lua_pushobject(L, v);
  lua_settable(L);
}


static void getinfo (lua_State *L) {
  lua_Debug ar;
  lua_Object res;
  lua_Object func = lua_getparam(L, 1);
  const char *options = luaL_opt_string(L, 2, "flnSu");
  char buff[20];
  if (lua_isnumber(L, func)) {
    if (!lua_getstack(L, (int)lua_getnumber(L, func), &ar)) {
      lua_pushnil(L);  /* level out of range */
      return;
    }
  }
  else if (lua_isfunction(L, func)) {
    ar.func = func;
    sprintf(buff, ">%.10s", options);
    options = buff;
  }
  else
    luaL_argerror(L, 1, "function or level expected");
  res = lua_createtable(L);
  if (!lua_getinfo(L, options, &ar))
    luaL_argerror(L, 2, "invalid option");
  for (; *options; options++) {
    switch (*options) {
      case 'S':
        settabss(L, res, "source", ar.source);
        settabsi(L, res, "linedefined", ar.linedefined);
        settabss(L, res, "what", ar.what);
        break;
      case 'l':
        settabsi(L, res, "currentline", ar.currentline);
        break;
      case 'u':
        settabsi(L, res, "nups", ar.nups);
        break;
      case 'n':
        settabss(L, res, "name", ar.name);
        settabss(L, res, "namewhat", ar.namewhat);
        break;
      case 'f':
        settabso(L, res, "func", ar.func);
        break;
    }
  }
  lua_pushobject(L, res);
}
    

static void getlocal (lua_State *L) {
  lua_Debug ar;
  lua_Localvar lvar;
  if (!lua_getstack(L, luaL_check_int(L, 1), &ar))  /* level out of range? */
    luaL_argerror(L, 1, "level out of range");
  lvar.index = luaL_check_int(L, 2);
  if (lua_getlocal(L, &ar, &lvar)) {
    lua_pushstring(L, lvar.name);
    lua_pushobject(L, lvar.value);
  }
  else lua_pushnil(L);
}


static void setlocal (lua_State *L) {
  lua_Debug ar;
  lua_Localvar lvar;
  if (!lua_getstack(L, luaL_check_int(L, 1), &ar))  /* level out of range? */
    luaL_argerror(L, 1, "level out of range");
  lvar.index = luaL_check_int(L, 2);
  lvar.value = luaL_nonnullarg(L, 3);
  if (lua_setlocal(L, &ar, &lvar))
    lua_pushstring(L, lvar.name);
  else lua_pushnil(L);
}


/*
** because of these variables, this module is not reentrant, and should
** not be used in multiple states
*/

static int linehook = LUA_NOREF;  /* Lua reference to line hook function */
static int callhook = LUA_NOREF;  /* Lua reference to call hook function */



static void linef (lua_State *L, lua_Debug *ar) {
  if (linehook != LUA_NOREF) {
    lua_pushnumber(L, ar->currentline);
    lua_callfunction(L, lua_getref(L, linehook));
  }
}


static void callf (lua_State *L, lua_Debug *ar) {
  if (callhook != LUA_NOREF) {
    lua_pushstring(L, ar->event);
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
  {"getlocal", getlocal},
  {"getinfo", getinfo},
  {"setcallhook", setcallhook},
  {"setlinehook", setlinehook},
  {"setlocal", setlocal}
};


void lua_dblibopen (lua_State *L) {
  luaL_openl(L, dblib);
}

