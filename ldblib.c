/*
** $Id: ldblib.c,v 1.19 2000/08/28 17:57:04 roberto Exp roberto $
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



static void settabss (lua_State *L, const char *i, const char *v) {
  lua_pushstring(L, i);
  lua_pushstring(L, v);
  lua_settable(L, -3);
}


static void settabsi (lua_State *L, const char *i, int v) {
  lua_pushstring(L, i);
  lua_pushnumber(L, v);
  lua_settable(L, -3);
}


static int getinfo (lua_State *L) {
  lua_Debug ar;
  const char *options = luaL_opt_string(L, 2, "flnSu");
  char buff[20];
  if (lua_isnumber(L, 1)) {
    if (!lua_getstack(L, (int)lua_tonumber(L, 1), &ar)) {
      lua_pushnil(L);  /* level out of range */
      return 1;
    }
  }
  else if (lua_isfunction(L, 1)) {
    lua_pushvalue(L, 1);
    sprintf(buff, ">%.10s", options);
    options = buff;
  }
  else
    luaL_argerror(L, 1, "function or level expected");
  if (!lua_getinfo(L, options, &ar))
    luaL_argerror(L, 2, "invalid option");
  lua_newtable(L);
  for (; *options; options++) {
    switch (*options) {
      case 'S':
        settabss(L, "source", ar.source);
        settabsi(L, "linedefined", ar.linedefined);
        settabss(L, "what", ar.what);
        break;
      case 'l':
        settabsi(L, "currentline", ar.currentline);
        break;
      case 'u':
        settabsi(L, "nups", ar.nups);
        break;
      case 'n':
        settabss(L, "name", ar.name);
        settabss(L, "namewhat", ar.namewhat);
        break;
      case 'f':
        lua_pushstring(L, "func");
        lua_pushvalue(L, -3);
        lua_settable(L, -3);
        break;
    }
  }
  return 1;  /* return table */
}
    

static int getlocal (lua_State *L) {
  lua_Debug ar;
  const char *name;
  if (!lua_getstack(L, luaL_check_int(L, 1), &ar))  /* level out of range? */
    luaL_argerror(L, 1, "level out of range");
  name = lua_getlocal(L, &ar, luaL_check_int(L, 2));
  if (name) {
    lua_pushstring(L, name);
    lua_pushvalue(L, -2);
    return 2;
  }
  else {
    lua_pushnil(L);
    return 1;
  }
}


static int setlocal (lua_State *L) {
  lua_Debug ar;
  if (!lua_getstack(L, luaL_check_int(L, 1), &ar))  /* level out of range? */
    luaL_argerror(L, 1, "level out of range");
  luaL_checktype(L, 3, "any");
  lua_pushstring(L, lua_setlocal(L, &ar, luaL_check_int(L, 2)));
  return 1;
}


/*
** because of these variables, this module is not reentrant, and should
** not be used in multiple states
*/

static int linehook = LUA_NOREF;  /* Lua reference to line hook function */
static int callhook = LUA_NOREF;  /* Lua reference to call hook function */



static void linef (lua_State *L, lua_Debug *ar) {
  if (linehook != LUA_NOREF) {
    lua_getref(L, linehook);
    lua_pushnumber(L, ar->currentline);
    lua_call(L, 1, 0);
  }
}


static void callf (lua_State *L, lua_Debug *ar) {
  if (callhook != LUA_NOREF) {
    lua_getref(L, callhook);
    lua_pushstring(L, ar->event);
    lua_call(L, 1, 0);
  }
}


static int setcallhook (lua_State *L) {
  lua_unref(L, callhook);
  if (lua_isnull(L, 1)) {
    callhook = LUA_NOREF;
    lua_setcallhook(L, NULL);
  }
  else {
    callhook = lua_ref(L, 1);
    lua_setcallhook(L, callf);
  }
  return 0;
}


static int setlinehook (lua_State *L) {
  lua_unref(L, linehook);
  if (lua_isnull(L, 1)) {
    linehook = LUA_NOREF;
    lua_setlinehook(L, NULL);
  }
  else {
    linehook = lua_ref(L, 1);
    lua_setlinehook(L, linef);
  }
  return 0;
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

