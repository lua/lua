/*
** $Id: lauxlib.c,v 1.43 2000/10/30 13:07:48 roberto Exp $
** Auxiliary functions for building Lua libraries
** See Copyright Notice in lua.h
*/


#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* This file uses only the official API of Lua.
** Any function declared here could be written as an application function.
** With care, these functions can be used by other libraries.
*/

#include "lua.h"

#include "lauxlib.h"
#include "luadebug.h"



LUALIB_API int luaL_findstring (const char *name, const char *const list[]) {
  int i;
  for (i=0; list[i]; i++)
    if (strcmp(list[i], name) == 0)
      return i;
  return -1;  /* name not found */
}

LUALIB_API void luaL_argerror (lua_State *L, int narg, const char *extramsg) {
  lua_Debug ar;
  lua_getstack(L, 0, &ar);
  lua_getinfo(L, "n", &ar);
  if (ar.name == NULL)
    ar.name = "?";
  luaL_verror(L, "bad argument #%d to `%.50s' (%.100s)",
              narg, ar.name, extramsg);
}


static void type_error (lua_State *L, int narg, int t) {
  char buff[50];
  sprintf(buff, "%.8s expected, got %.8s", lua_typename(L, t),
                                           lua_typename(L, lua_type(L, narg)));
  luaL_argerror(L, narg, buff);
}


LUALIB_API void luaL_checkstack (lua_State *L, int space, const char *mes) {
  if (space > lua_stackspace(L))
    luaL_verror(L, "stack overflow (%.30s)", mes);
}


LUALIB_API void luaL_checktype(lua_State *L, int narg, int t) {
  if (lua_type(L, narg) != t)
    type_error(L, narg, t);
}


LUALIB_API void luaL_checkany (lua_State *L, int narg) {
  if (lua_type(L, narg) == LUA_TNONE)
    luaL_argerror(L, narg, "value expected");
}


LUALIB_API const char *luaL_check_lstr (lua_State *L, int narg, size_t *len) {
  const char *s = lua_tostring(L, narg);
  if (!s) type_error(L, narg, LUA_TSTRING);
  if (len) *len = lua_strlen(L, narg);
  return s;
}


LUALIB_API const char *luaL_opt_lstr (lua_State *L, int narg, const char *def, size_t *len) {
  if (lua_isnull(L, narg)) {
    if (len)
      *len = (def ? strlen(def) : 0);
    return def;
  }
  else return luaL_check_lstr(L, narg, len);
}


LUALIB_API double luaL_check_number (lua_State *L, int narg) {
  double d = lua_tonumber(L, narg);
  if (d == 0 && !lua_isnumber(L, narg))  /* avoid extra test when d is not 0 */
    type_error(L, narg, LUA_TNUMBER);
  return d;
}


LUALIB_API double luaL_opt_number (lua_State *L, int narg, double def) {
  if (lua_isnull(L, narg)) return def;
  else return luaL_check_number(L, narg);
}


LUALIB_API void luaL_openlib (lua_State *L, const struct luaL_reg *l, int n) {
  int i;
  for (i=0; i<n; i++)
    lua_register(L, l[i].name, l[i].func);
}


LUALIB_API void luaL_verror (lua_State *L, const char *fmt, ...) {
  char buff[500];
  va_list argp;
  va_start(argp, fmt);
  vsprintf(buff, fmt, argp);
  va_end(argp);
  lua_error(L, buff);
}


/*
** {======================================================
** Generic Buffer manipulation
** =======================================================
*/


#define buffempty(B)	((B)->p == (B)->buffer)
#define bufflen(B)	((B)->p - (B)->buffer)
#define bufffree(B)	((size_t)(LUAL_BUFFERSIZE - bufflen(B)))

#define LIMIT	(LUA_MINSTACK/2)


static int emptybuffer (luaL_Buffer *B) {
  size_t l = bufflen(B);
  if (l == 0) return 0;  /* put nothing on stack */
  else {
    lua_pushlstring(B->L, B->buffer, l);
    B->p = B->buffer;
    B->level++;
    return 1;
  }
}


static void adjuststack (luaL_Buffer *B) {
  if (B->level > 1) {
    lua_State *L = B->L;
    int toget = 1;  /* number of levels to concat */
    size_t toplen = lua_strlen(L, -1);
    do {
      size_t l = lua_strlen(L, -(toget+1));
      if (B->level - toget + 1 >= LIMIT || toplen > l) {
        toplen += l;
        toget++;
      }
      else break;
    } while (toget < B->level);
    if (toget >= 2) {
      lua_concat(L, toget);
      B->level = B->level - toget + 1;
    }
  }
}


LUALIB_API char *luaL_prepbuffer (luaL_Buffer *B) {
  if (emptybuffer(B))
    adjuststack(B);
  return B->buffer;
}


LUALIB_API void luaL_addlstring (luaL_Buffer *B, const char *s, size_t l) {
  while (l--)
    luaL_putchar(B, *s++);
}


LUALIB_API void luaL_addstring (luaL_Buffer *B, const char *s) {
  luaL_addlstring(B, s, strlen(s));
}


LUALIB_API void luaL_pushresult (luaL_Buffer *B) {
  emptybuffer(B);
  if (B->level == 0)
    lua_pushlstring(B->L, NULL, 0);
  else if (B->level > 1)
    lua_concat(B->L, B->level);
  B->level = 1;
}


LUALIB_API void luaL_addvalue (luaL_Buffer *B) {
  lua_State *L = B->L;
  size_t vl = lua_strlen(L, -1);
  if (vl <= bufffree(B)) {  /* fit into buffer? */
    memcpy(B->p, lua_tostring(L, -1), vl);  /* put it there */
    B->p += vl;
    lua_pop(L, 1);  /* remove from stack */
  }
  else {
    if (emptybuffer(B))
      lua_insert(L, -2);  /* put buffer before new value */
    B->level++;  /* add new value into B stack */
    adjuststack(B);
  }
}


LUALIB_API void luaL_buffinit (lua_State *L, luaL_Buffer *B) {
  B->L = L;
  B->p = B->buffer;
  B->level = 0;
}

/* }====================================================== */
