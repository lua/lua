/*
** $Id: stubs.c,v 1.20 2000/10/31 16:57:23 lhf Exp $
** avoid runtime modules in luac
** See Copyright Notice in lua.h
*/

#include <stdio.h>
#include <stdlib.h>

#include "ldo.h"
#include "llex.h"
#include "luac.h"
#undef L

#ifndef NOSTUBS

const char luac_ident[] = "$luac: " LUA_VERSION " " LUA_COPYRIGHT " $\n"
                          "$Authors: " LUA_AUTHORS " $";

/*
* avoid lapi ldebug ldo lgc lstate ltm lvm
* use only lcode lfunc llex lmem lobject lparser lstring ltable lzio
*/

/* simplified from ldo.c */
void lua_error (lua_State* L, const char* s) {
  UNUSED(L);
  if (s) fprintf(stderr,"luac: %s\n",s);
  exit(1);
}

/* simplified from ldo.c */
void luaD_breakrun (lua_State *L, int errcode) {
  UNUSED(errcode);
  lua_error(L,"memory allocation error");
}

/* simplified from lstate.c */
lua_State *lua_open (int stacksize) {
  lua_State *L = luaM_new(NULL, lua_State);
  if (L == NULL) return NULL;  /* memory allocation error */
  L->stack = NULL;
  L->strt.size = L->udt.size = 0;
  L->strt.nuse = L->udt.nuse = 0;
  L->strt.hash = NULL;
  L->udt.hash = NULL;
  L->Mbuffer = NULL;
  L->Mbuffsize = 0;
  L->rootproto = NULL;
  L->rootcl = NULL;
  L->roottable = NULL;
  L->TMtable = NULL;
  L->last_tag = -1;
  L->refArray = NULL;
  L->refSize = 0;
  L->refFree = NONEXT;
  L->nblocks = sizeof(lua_State);
  L->GCthreshold = MAX_INT;  /* to avoid GC during pre-definitions */
  L->callhook = NULL;
  L->linehook = NULL;
  L->allowhooks = 1;
  L->errorJmp = NULL;
  if (stacksize == 0)
    stacksize = DEFAULT_STACK_SIZE;
  else
    stacksize += LUA_MINSTACK;
  L->gt = luaH_new(L, 10);  /* table of globals */
  luaS_init(L);
  luaX_init(L);
  L->GCthreshold = 2*L->nblocks;
  return L;
}

/* copied from ldebug.c */
int luaG_getline (int *lineinfo, int pc, int refline, int *prefi) {
  int refi;
  if (lineinfo == NULL || pc == -1)
    return -1;  /* no line info or function is not active */
  refi = prefi ? *prefi : 0;
  if (lineinfo[refi] < 0)
    refline += -lineinfo[refi++]; 
  LUA_ASSERT(lineinfo[refi] >= 0, "invalid line info");
  while (lineinfo[refi] > pc) {
    refline--;
    refi--;
    if (lineinfo[refi] < 0)
      refline -= -lineinfo[refi--]; 
    LUA_ASSERT(lineinfo[refi] >= 0, "invalid line info");
  }
  for (;;) {
    int nextline = refline + 1;
    int nextref = refi + 1;
    if (lineinfo[nextref] < 0)
      nextline += -lineinfo[nextref++]; 
    LUA_ASSERT(lineinfo[nextref] >= 0, "invalid line info");
    if (lineinfo[nextref] > pc)
      break;
    refline = nextline;
    refi = nextref;
  }
  if (prefi) *prefi = refi;
  return refline;
}

/*
* the code below avoids the lexer and the parser (llex lparser lcode).
* it is useful if you only want to load binary files.
* this works for interpreters like lua.c too.
*/

#ifdef NOPARSER

#include "llex.h"
#include "lparser.h"

void luaX_init(lua_State *L) {
  UNUSED(L);
}

Proto *luaY_parser(lua_State *L, ZIO *z) {
  UNUSED(z);
  lua_error(L,"parser not loaded");
  return NULL;
}

#endif
#endif
