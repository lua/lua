/*
** $Id: lbuffer.c,v 1.4 1998/06/19 16:14:09 roberto Exp $
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

#define BUFF_STEP	32

#define openspace(size)  if (L->Mbuffnext+(size) > L->Mbuffsize) Openspace(size)

static void Openspace (int size)
{
  lua_State *l = L;  /* to optimize */
  int base = l->Mbuffbase-l->Mbuffer;
  l->Mbuffsize *= 2;
  if (l->Mbuffnext+size > l->Mbuffsize)  /* still not big enough? */
    l->Mbuffsize = l->Mbuffnext+size;
  l->Mbuffer = luaM_realloc(l->Mbuffer, l->Mbuffsize);
  l->Mbuffbase = l->Mbuffer+base;
}


char *luaL_openspace (int size)
{
  openspace(size);
  return L->Mbuffer+L->Mbuffnext;
}


void luaL_addchar (int c)
{
  openspace(BUFF_STEP);
  L->Mbuffer[L->Mbuffnext++] = c;
}


void luaL_resetbuffer (void)
{
  L->Mbuffnext = L->Mbuffbase-L->Mbuffer;
}


void luaL_addsize (int n)
{
  L->Mbuffnext += n;
}

int luaL_getsize (void)
{
  return L->Mbuffnext-(L->Mbuffbase-L->Mbuffer);
}

int luaL_newbuffer (int size)
{
  int old = L->Mbuffbase-L->Mbuffer;
  openspace(size);
  L->Mbuffbase = L->Mbuffer+L->Mbuffnext;
  return old;
}


void luaL_oldbuffer (int old)
{
  L->Mbuffnext = L->Mbuffbase-L->Mbuffer;
  L->Mbuffbase = L->Mbuffer+old;
}


char *luaL_buffer (void)
{
  return L->Mbuffbase;
}

