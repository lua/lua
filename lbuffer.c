/*
** $Id: lbuffer.c,v 1.13 2000/05/24 13:54:49 roberto Exp roberto $
** Auxiliary functions for building Lua libraries
** See Copyright Notice in lua.h
*/


#include <stdio.h>

#define LUA_REENTRANT

#include "lua.h"

#include "lauxlib.h"
#include "lmem.h"
#include "lstate.h"


/*-------------------------------------------------------
**  Auxiliary buffer
-------------------------------------------------------*/


/*
** amount of extra space (pre)allocated when buffer is reallocated
*/
#define EXTRABUFF	32


#define openspace(L, size)  if ((size_t)(size) > L->Mbuffsize-L->Mbuffnext) \
                              Openspace(L, size)

static void Openspace (lua_State *L, size_t size) {
  lint32 newsize = ((lint32)L->Mbuffnext+size+EXTRABUFF)*2;
  luaM_reallocvector(L, L->Mbuffer, newsize, char);
  L->Mbuffsize = newsize;
}


char *luaL_openspace (lua_State *L, size_t size) {
  openspace(L, size);
  return L->Mbuffer+L->Mbuffnext;
}


void luaL_addchar (lua_State *L, int c) {
  openspace(L, 1);
  L->Mbuffer[L->Mbuffnext++] = (char)c;
}


void luaL_resetbuffer (lua_State *L) {
  L->Mbuffnext = L->Mbuffbase;
}


void luaL_addsize (lua_State *L, size_t n) {
  L->Mbuffnext += n;
}

size_t luaL_getsize (lua_State *L) {
  return L->Mbuffnext-L->Mbuffbase;
}

size_t luaL_newbuffer (lua_State *L, size_t size) {
  size_t old = L->Mbuffbase;
  openspace(L, size);
  L->Mbuffbase = L->Mbuffnext;
  return old;
}


void luaL_oldbuffer (lua_State *L, size_t old) {
  L->Mbuffnext = L->Mbuffbase;
  L->Mbuffbase = old;
}


char *luaL_buffer (lua_State *L) {
  return L->Mbuffer+L->Mbuffbase;
}

