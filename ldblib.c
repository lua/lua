/*
** $Id: ldblib.c,v 1.63 2002/07/08 20:22:08 roberto Exp roberto $
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
  lua_rawset(L, -3);
}


static void settabsi (lua_State *L, const char *i, int v) {
  lua_pushstring(L, i);
  lua_pushnumber(L, v);
  lua_rawset(L, -3);
}


static int getinfo (lua_State *L) {
  lua_Debug ar;
  const char *options = luaL_opt_string(L, 2, "flnSu");
  if (lua_isnumber(L, 1)) {
    if (!lua_getstack(L, (int)(lua_tonumber(L, 1)), &ar)) {
      lua_pushnil(L);  /* level out of range */
      return 1;
    }
  }
  else if (lua_isfunction(L, 1)) {
    lua_pushfstring(L, ">%s", options);
    options = lua_tostring(L, -1);
    lua_pushvalue(L, 1);
  }
  else
    return luaL_argerror(L, 1, "function or level expected");
  if (!lua_getinfo(L, options, &ar))
    return luaL_argerror(L, 2, "invalid option");
  lua_newtable(L);
  for (; *options; options++) {
    switch (*options) {
      case 'S':
        settabss(L, "source", ar.source);
        settabss(L, "short_src", ar.short_src);
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
        lua_pushliteral(L, "func");
        lua_pushvalue(L, -3);
        lua_rawset(L, -3);
        break;
    }
  }
  return 1;  /* return table */
}
    

static int getlocal (lua_State *L) {
  lua_Debug ar;
  const char *name;
  if (!lua_getstack(L, luaL_check_int(L, 1), &ar))  /* level out of range? */
    return luaL_argerror(L, 1, "level out of range");
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
    return luaL_argerror(L, 1, "level out of range");
  luaL_check_any(L, 3);
  lua_pushstring(L, lua_setlocal(L, &ar, luaL_check_int(L, 2)));
  return 1;
}



static const char KEY_HOOK = 'h';


static void hookf (lua_State *L, lua_Debug *ar) {
  static const char *const hooknames[] = {"call", "return", "line", "count"};
  lua_pushlightuserdata(L, (void *)&KEY_HOOK);
  lua_rawget(L, LUA_REGISTRYINDEX);
  if (lua_isfunction(L, -1)) {
    lua_pushstring(L, hooknames[(int)ar->event]);
    if (ar->currentline >= 0) lua_pushnumber(L, ar->currentline);
    else lua_pushnil(L);
    lua_assert(lua_getinfo(L, "lS", ar));
    lua_call(L, 2, 0);
  }
  else
    lua_pop(L, 1);  /* pop result from gettable */
}


static int makemask (const char *smask, int count) {
  int mask = 0;
  if (strchr(smask, 'c')) mask |= LUA_MASKCALL;
  if (strchr(smask, 'r')) mask |= LUA_MASKRET;
  if (strchr(smask, 'l')) mask |= LUA_MASKLINE;
  return mask | LUA_MASKCOUNT(count);
}


static char *unmakemask (int mask, char *smask) {
  int i = 0;
  if (mask & LUA_MASKCALL) smask[i++] = 'c';
  if (mask & LUA_MASKRET) smask[i++] = 'r';
  if (mask & LUA_MASKLINE) smask[i++] = 'l';
  smask[i] = '\0';
  return smask;
}


static int sethook (lua_State *L) {
  if (lua_isnoneornil(L, 1)) {
    lua_settop(L, 1);
    lua_sethook(L, NULL, 0);  /* turn off hooks */
  }
  else {
    const char *smask = luaL_check_string(L, 2);
    int count = luaL_opt_int(L, 3, 0);
    luaL_check_type(L, 1, LUA_TFUNCTION);
    lua_sethook(L, hookf, makemask(smask, count));
  }
  lua_pushlightuserdata(L, (void *)&KEY_HOOK);
  lua_pushvalue(L, 1);
  lua_rawset(L, LUA_REGISTRYINDEX);  /* set new hook */
  return 0;
}


static int gethook (lua_State *L) {
  char buff[5];
  int mask = lua_gethookmask(L);
  lua_pushlightuserdata(L, (void *)&KEY_HOOK);
  lua_rawget(L, LUA_REGISTRYINDEX);   /* get hook */
  lua_pushstring(L, unmakemask(mask, buff));
  lua_pushnumber(L, lua_getmaskcount(mask));
  return 3;
}


static int debug (lua_State *L) {
  for (;;) {
    char buffer[250];
    fputs("lua_debug> ", stderr);
    if (fgets(buffer, sizeof(buffer), stdin) == 0 ||
        strcmp(buffer, "cont\n") == 0)
      return 0;
    lua_dostring(L, buffer);
    lua_settop(L, 0);  /* remove eventual returns */
  }
}


#define LEVELS1	12	/* size of the first part of the stack */
#define LEVELS2	10	/* size of the second part of the stack */

static int errorfb (lua_State *L) {
  int level = 1;  /* skip level 0 (it's this function) */
  int firstpart = 1;  /* still before eventual `...' */
  int alllevels = 1;
  const char *msg = lua_tostring(L, 1);
  lua_Debug ar;
  lua_settop(L, 0);
  if (msg) {
    alllevels = 0;
    if (!strstr(msg, "stack traceback:\n"))
      lua_pushliteral(L, "stack traceback:\n");
  }
  while (lua_getstack(L, level++, &ar)) {
    if (level > LEVELS1 && firstpart) {
      /* no more than `LEVELS2' more levels? */
      if (!lua_getstack(L, level+LEVELS2, &ar))
        level--;  /* keep going */
      else {
        lua_pushliteral(L, "\t...\n");  /* too many levels */
        while (lua_getstack(L, level+LEVELS2, &ar))  /* find last levels */
          level++;
      }
      firstpart = 0;
      continue;
    }
    lua_pushliteral(L, "\t");
    lua_getinfo(L, "Snlc", &ar);
    lua_pushfstring(L, "%s:", ar.short_src);
    if (ar.currentline > 0)
      lua_pushfstring(L, "%d:", ar.currentline);
    switch (*ar.namewhat) {
      case 'g':  /* global */ 
      case 'l':  /* local */
      case 'f':  /* field */
      case 'm':  /* method */
        lua_pushfstring(L, " in function `%s'", ar.name);
        break;
      default: {
        if (*ar.what == 'm')  /* main? */
          lua_pushfstring(L, " in main chunk");
        else if (*ar.what == 'C')  /* C function? */
          lua_pushfstring(L, "%s", ar.short_src);
        else
          lua_pushfstring(L, " in function <%s:%d>",
                             ar.short_src, ar.linedefined);
      }
    }
    lua_pushliteral(L, "\n");
    lua_concat(L, lua_gettop(L));
    if (!alllevels && ar.isprotected) break;
  }
  lua_concat(L, lua_gettop(L));
  return 1;
}


static const luaL_reg dblib[] = {
  {"getlocal", getlocal},
  {"getinfo", getinfo},
  {"gethook", gethook},
  {"sethook", sethook},
  {"setlocal", setlocal},
  {"debug", debug},
  {"traceback", errorfb},
  {NULL, NULL}
};


LUALIB_API int lua_dblibopen (lua_State *L) {
  luaL_opennamedlib(L, LUA_DBLIBNAME, dblib, 0);
  lua_pushliteral(L, LUA_TRACEBACK);
  lua_pushcfunction(L, errorfb);
  lua_settable(L, LUA_REGISTRYINDEX);
  return 0;
}

