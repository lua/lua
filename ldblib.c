/*
** $Id: ldblib.c,v 1.34 2001/03/06 20:09:38 roberto Exp roberto $
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



static void settabss (lua_State *L, const l_char *i, const l_char *v) {
  lua_pushstring(L, i);
  lua_pushstring(L, v);
  lua_settable(L, -3);
}


static void settabsi (lua_State *L, const l_char *i, int v) {
  lua_pushstring(L, i);
  lua_pushnumber(L, v);
  lua_settable(L, -3);
}


static int getinfo (lua_State *L) {
  lua_Debug ar;
  const l_char *options = luaL_opt_string(L, 2, l_s("flnSu"));
  l_char buff[20];
  if (lua_isnumber(L, 1)) {
    if (!lua_getstack(L, (int)lua_tonumber(L, 1), &ar)) {
      lua_pushnil(L);  /* level out of range */
      return 1;
    }
  }
  else if (lua_isfunction(L, 1)) {
    lua_pushvalue(L, 1);
    sprintf(buff, l_s(">%.10s"), options);
    options = buff;
  }
  else
    luaL_argerror(L, 1, l_s("function or level expected"));
  if (!lua_getinfo(L, options, &ar))
    luaL_argerror(L, 2, l_s("invalid option"));
  lua_newtable(L);
  for (; *options; options++) {
    switch (*options) {
      case l_c('S'):
        settabss(L, l_s("source"), ar.source);
        if (ar.source)
          settabss(L, l_s("short_src"), ar.short_src);
        settabsi(L, l_s("linedefined"), ar.linedefined);
        settabss(L, l_s("what"), ar.what);
        break;
      case l_c('l'):
        settabsi(L, l_s("currentline"), ar.currentline);
        break;
      case l_c('u'):
        settabsi(L, l_s("nups"), ar.nups);
        break;
      case l_c('n'):
        settabss(L, l_s("name"), ar.name);
        settabss(L, l_s("namewhat"), ar.namewhat);
        break;
      case l_c('f'):
        lua_pushliteral(L, l_s("func"));
        lua_pushvalue(L, -3);
        lua_settable(L, -3);
        break;
    }
  }
  return 1;  /* return table */
}
    

static int getlocal (lua_State *L) {
  lua_Debug ar;
  const l_char *name;
  if (!lua_getstack(L, luaL_check_int(L, 1), &ar))  /* level out of range? */
    luaL_argerror(L, 1, l_s("level out of range"));
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
    luaL_argerror(L, 1, l_s("level out of range"));
  luaL_checkany(L, 3);
  lua_pushstring(L, lua_setlocal(L, &ar, luaL_check_int(L, 2)));
  return 1;
}



/* dummy variables (to define unique addresses) */
static const l_char key1[] = l_s("ab");
#define KEY_CALLHOOK	((void *)key1)
#define KEY_LINEHOOK	((void *)(key1+1))


static void hookf (lua_State *L, void *key) {
  lua_getregistry(L);
  lua_pushuserdata(L, key);
  lua_gettable(L, -2);
  if (lua_isfunction(L, -1)) {
    lua_pushvalue(L, -3);  /* original argument (below table and function) */
    lua_rawcall(L, 1, 0);
  }
  else
    lua_pop(L, 1);  /* pop result from gettable */
  lua_pop(L, 1);  /* pop table */
}


static void callf (lua_State *L, lua_Debug *ar) {
  lua_pushstring(L, ar->event);
  hookf(L, KEY_CALLHOOK);
}


static void linef (lua_State *L, lua_Debug *ar) {
  lua_pushnumber(L, ar->currentline);
  hookf(L, KEY_LINEHOOK);
}


static void sethook (lua_State *L, void *key, lua_Hook hook,
                     lua_Hook (*sethookf)(lua_State * L, lua_Hook h)) {
  lua_settop(L, 1);
  if (lua_isnil(L, 1))
    (*sethookf)(L, NULL);
  else if (lua_isfunction(L, 1))
    (*sethookf)(L, hook);
  else
    luaL_argerror(L, 1, l_s("function expected"));
  lua_getregistry(L);
  lua_pushuserdata(L, key);
  lua_pushvalue(L, -1);  /* dup key */
  lua_gettable(L, -3);   /* get old value */
  lua_pushvalue(L, -2);  /* key (again) */
  lua_pushvalue(L, 1);
  lua_settable(L, -5);  /* set new value */
}


static int setcallhook (lua_State *L) {
  sethook(L, KEY_CALLHOOK, callf, lua_setcallhook);
  return 1;
}


static int setlinehook (lua_State *L) {
  sethook(L, KEY_LINEHOOK, linef, lua_setlinehook);
  return 1;
}


static const luaL_reg dblib[] = {
  {l_s("getlocal"), getlocal},
  {l_s("getinfo"), getinfo},
  {l_s("setcallhook"), setcallhook},
  {l_s("setlinehook"), setlinehook},
  {l_s("setlocal"), setlocal}
};


LUALIB_API int lua_dblibopen (lua_State *L) {
  luaL_openl(L, dblib);
  return 0;
}

