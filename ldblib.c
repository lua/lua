/*
** $Id: ldblib.c,v 1.11 2000/03/03 14:58:26 roberto Exp roberto $
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


static void settabso (lua_State *L, lua_Object t, const char *i, lua_Object v) {
  lua_pushobject(L, t);
  lua_pushstring(L, i);
  lua_pushobject(L, v);
  lua_settable(L);
}


static void getstack (lua_State *L) {
  lua_Debug ar;
  if (!lua_getstack(L, luaL_check_int(L, 1), &ar))  /* level out of range? */
    return;
  else {
    const char *options = luaL_check_string(L, 2);
    lua_Object res = lua_createtable(L);
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
  {"getstack", getstack},
  {"setcallhook", setcallhook},
  {"setlinehook", setlinehook},
  {"setlocal", setlocal}
};


void lua_dblibopen (lua_State *L) {
  luaL_openl(L, dblib);
}

