/*
** $Id: lbuffer.c,v 1.10 1999/11/10 15:40:46 roberto Exp roberto $
** Auxiliary functions for building Lua libraries
** See Copyright Notice in lua.h
*/


#include <stdio.h>

#define LUA_REENTRANT

#include "lauxlib.h"
#include "lmem.h"
#include "lstate.h"


/*-------------------------------------------------------
**  Auxiliary buffer
-------------------------------------------------------*/


#define EXTRABUFF	32


#define openspace(L, size)  if (L->Mbuffnext+(size) > L->Mbuffsize) Openspace(L, size)

static void Openspace (lua_State *L, int size) {
  L->Mbuffsize = (L->Mbuffnext+size+EXTRABUFF)*2;
  luaM_reallocvector(L, L->Mbuffer, L->Mbuffsize, char);
}


char *luaL_openspace (lua_State *L, int size) {
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


void luaL_addsize (lua_State *L, int n) {
  L->Mbuffnext += n;
}

int luaL_getsize (lua_State *L) {
  return L->Mbuffnext-L->Mbuffbase;
}

int luaL_newbuffer (lua_State *L, int size) {
  int old = L->Mbuffbase;
  openspace(L, size);
  L->Mbuffbase = L->Mbuffnext;
  return old;
}


void luaL_oldbuffer (lua_State *L, int old) {
  L->Mbuffnext = L->Mbuffbase;
  L->Mbuffbase = old;
}


char *luaL_buffer (lua_State *L) {
  return L->Mbuffer+L->Mbuffbase;
}

