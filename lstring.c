/*
** $Id: lstring.c,v 1.62 2001/03/26 14:31:49 roberto Exp roberto $
** String table (keeps all strings handled by Lua)
** See Copyright Notice in lua.h
*/


#include <string.h>

#define LUA_PRIVATE
#include "lua.h"

#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"



void luaS_freeall (lua_State *L) {
  lua_assert(G(L)->strt.nuse==0);
  luaM_freearray(L, G(L)->strt.hash, G(L)->strt.size, TString *);
}


void luaS_resize (lua_State *L, int newsize) {
  TString **newhash = luaM_newvector(L, newsize, TString *);
  stringtable *tb = &G(L)->strt;
  int i;
  for (i=0; i<newsize; i++) newhash[i] = NULL;
  /* rehash */
  for (i=0; i<tb->size; i++) {
    TString *p = tb->hash[i];
    while (p) {  /* for each node in the list */
      TString *next = p->nexthash;  /* save next */
      lu_hash h = p->hash;
      int h1 = lmod(h, newsize);  /* new position */
      lua_assert((int)(h%newsize) == lmod(h, newsize));
      p->nexthash = newhash[h1];  /* chain it in new position */
      newhash[h1] = p;
      p = next;
    }
  }
  luaM_freearray(L, tb->hash, tb->size, TString *);
  tb->size = newsize;
  tb->hash = newhash;
}


TString *luaS_newlstr (lua_State *L, const l_char *str, size_t l) {
  TString *ts;
  lu_hash h = l;  /* seed */
  size_t step = (l>>5)+1;  /* if string is too long, don't hash all its chars */
  size_t l1;
  for (l1=l; l1>=step; l1-=step)  /* compute hash */
    h = h ^ ((h<<5)+(h>>2)+uchar(str[l1-1]));
  for (ts = G(L)->strt.hash[lmod(h, G(L)->strt.size)]; ts; ts = ts->nexthash) {
    if (ts->len == l && (memcmp(str, getstr(ts), l) == 0))
      return ts;
  }
  /* not found */
  ts = (TString *)luaM_malloc(L, sizestring(l));
  ts->marked = 0;
  ts->nexthash = NULL;
  ts->len = l;
  ts->hash = h;
  ts->constindex = 0;
  memcpy(getstr(ts), str, l*sizeof(l_char));
  getstr(ts)[l] = 0;  /* ending 0 */
  h = lmod(h, G(L)->strt.size);
  ts->nexthash = G(L)->strt.hash[h];  /* chain new entry */
  G(L)->strt.hash[h] = ts;
  G(L)->strt.nuse++;
  if (G(L)->strt.nuse > (ls_nstr)G(L)->strt.size &&
      G(L)->strt.size <= MAX_INT/2)
    luaS_resize(L, G(L)->strt.size*2);  /* too crowded */
  return ts;
}


Udata *luaS_newudata (lua_State *L, size_t s) {
  Udata *u = (Udata *)luaM_malloc(L, sizeudata(s));
  u->marked = 0;
  u->len = s;
  u->tag = 0;
  u->value = ((union L_UUdata *)(u) + 1);
  /* chain it on udata list */
  u->next = G(L)->rootudata;
  G(L)->rootudata = u;
  return u;
}

