/*
** $Id: lstring.c,v 1.72 2002/02/08 22:41:09 roberto Exp roberto $
** String table (keeps all strings handled by Lua)
** See Copyright Notice in lua.h
*/


#include <string.h>

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
      TString *next = p->tsv.nexthash;  /* save next */
      lu_hash h = p->tsv.hash;
      int h1 = lmod(h, newsize);  /* new position */
      lua_assert(cast(int, h%newsize) == lmod(h, newsize));
      p->tsv.nexthash = newhash[h1];  /* chain it in new position */
      newhash[h1] = p;
      p = next;
    }
  }
  luaM_freearray(L, tb->hash, tb->size, TString *);
  tb->size = newsize;
  tb->hash = newhash;
}


static TString *newlstr (lua_State *L, const char *str, size_t l, lu_hash h) {
  TString *ts = cast(TString *, luaM_malloc(L, sizestring(l)));
  stringtable *tb;
  ts->tsv.nexthash = NULL;
  ts->tsv.len = l;
  ts->tsv.hash = h;
  ts->tsv.marked = 0;
  memcpy(ts+1, str, l*sizeof(char));
  ((char *)(ts+1))[l] = '\0';  /* ending 0 */
  tb = &G(L)->strt;
  h = lmod(h, tb->size);
  ts->tsv.nexthash = tb->hash[h];  /* chain new entry */
  tb->hash[h] = ts;
  tb->nuse++;
  if (tb->nuse > cast(ls_nstr, tb->size) && tb->size <= MAX_INT/2)
    luaS_resize(L, tb->size*2);  /* too crowded */
  return ts;
}


TString *luaS_newlstr (lua_State *L, const char *str, size_t l) {
  TString *ts;
  lu_hash h = (lu_hash)l;  /* seed */
  size_t step = (l>>5)+1;  /* if string is too long, don't hash all its chars */
  size_t l1;
  for (l1=l; l1>=step; l1-=step)  /* compute hash */
    h = h ^ ((h<<5)+(h>>2)+(unsigned char)(str[l1-1]));
  for (ts = G(L)->strt.hash[lmod(h, G(L)->strt.size)];
       ts != NULL;
       ts = ts->tsv.nexthash) {
    if (ts->tsv.len == l && (memcmp(str, getstr(ts), l) == 0))
      return ts;
  }
  return newlstr(L, str, l, h);  /* not found */
}


Udata *luaS_newudata (lua_State *L, size_t s) {
  Udata *u;
  s = (s+3) & (~(size_t)3);  /* make sure size is multiple of 4 */
  u = cast(Udata *, luaM_malloc(L, sizeudata(s)));
  u->uv.len = s;
  u->uv.metatable = hvalue(defaultmeta(L));
  u->uv.value = u + 1;
  /* chain it on udata list */
  u->uv.next = G(L)->rootudata;
  G(L)->rootudata = u;
  return u;
}

