/*
** $Id: lbuffer.c,v 1.9 1999/02/26 15:48:55 roberto Exp $
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


#define EXTRABUFF	32


#define openspace(size)  if (L->Mbuffnext+(size) > L->Mbuffsize) Openspace(size)

static void Openspace (int size) {
  lua_State *l = L;  /* to optimize */
  size += EXTRABUFF;
  l->Mbuffsize = l->Mbuffnext+size;
  luaM_growvector(l->Mbuffer, l->Mbuffnext, size, char, arrEM, MAX_INT);
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

