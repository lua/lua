/*
** $Id: lbuffer.c,v 1.5 1998/12/28 13:44:54 roberto Exp roberto $
** Auxiliary functions for building Lua libraries
** See Copyright Notice in lua.h
*/


#include <stdio.h>

#include "lauxlib.h"
#include "lmem.h"
#include "lstate.h"


/*-------------------------------------------------------
**  Auxiliary buffer
-------------------------------------------------------*/


#define openspace(size)  if (L->Mbuffnext+(size) > L->Mbuffsize) Openspace(size)

static void Openspace (int size) {
  lua_State *l = L;  /* to optimize */
  l->Mbuffsize = l->Mbuffnext+size;
  l->Mbuffer =  luaM_growvector(l->Mbuffer, l->Mbuffnext, size, char,
                                memEM, MAX_INT);
}


char *luaL_openspace (int size) {
  openspace(size);
  return L->Mbuffer+L->Mbuffnext;
}


void luaL_addchar (int c) {
  openspace(1);
  L->Mbuffer[L->Mbuffnext++] = (char)c;
}


void luaL_resetbuffer (void) {
  L->Mbuffnext = L->Mbuffbase;
}


void luaL_addsize (int n) {
  L->Mbuffnext += n;
}

int luaL_getsize (void) {
  return L->Mbuffnext-L->Mbuffbase;
}

int luaL_newbuffer (int size) {
  int old = L->Mbuffbase;
  openspace(size);
  L->Mbuffbase = L->Mbuffnext;
  return old;
}


void luaL_oldbuffer (int old) {
  L->Mbuffnext = L->Mbuffbase;
  L->Mbuffbase = old;
}


char *luaL_buffer (void) {
  return L->Mbuffer+L->Mbuffbase;
}

