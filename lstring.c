/*
** $Id: lstring.c,v 1.60 2001/02/22 17:15:18 roberto Exp roberto $
** String table (keeps all strings handled by Lua)
** See Copyright Notice in lua.h
*/


#include <string.h>

#include "lua.h"

#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"



void luaS_init (lua_State *L) {
  luaS_resize(L, &G(L)->strt, MINPOWER2);
  luaS_resize(L, &G(L)->udt, MINPOWER2);
}


void luaS_freeall (lua_State *L) {
  lua_assert(G(L)->strt.nuse==0);
  luaM_freearray(L, G(L)->strt.hash, G(L)->strt.size, TString *);
  lua_assert(G(L)->udt.nuse==0);
  luaM_freearray(L, G(L)->udt.hash, G(L)->udt.size, TString *);
}


void luaS_resize (lua_State *L, stringtable *tb, int newsize) {
  TString **newhash = luaM_newvector(L, newsize, TString *);
  int i;
  for (i=0; i<newsize; i++) newhash[i] = NULL;
  /* rehash */
  for (i=0; i<tb->size; i++) {
    TString *p = tb->hash[i];
    while (p) {  /* for each node in the list */
      TString *next = p->nexthash;  /* save next */
      lu_hash h = (tb == &G(L)->strt) ? p->u.s.hash : IntPoint(p->u.d.value);
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


static void newentry (lua_State *L, stringtable *tb, TString *ts, int h) {
  ts->nexthash = tb->hash[h];  /* chain new entry */
  tb->hash[h] = ts;
  tb->nuse++;
  if (tb->nuse > (ls_nstr)tb->size && tb->size <= MAX_INT/2)  /* too crowded? */
    luaS_resize(L, tb, tb->size*2);
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
  ts->u.s.hash = h;
  ts->u.s.constindex = 0;
  memcpy(getstr(ts), str, l*sizeof(l_char));
  getstr(ts)[l] = 0;  /* ending 0 */
  newentry(L, &G(L)->strt, ts, lmod(h, G(L)->strt.size));  /* insert it */
  return ts;
}


TString *luaS_newudata (lua_State *L, size_t s, void *udata) {
  TString *ts = (TString *)luaM_malloc(L, sizeudata(s));
  ts->marked = 0;
  ts->nexthash = NULL;
  ts->len = s;
  ts->u.d.tag = 0;
  ts->u.d.value = (s > 0) ? getstr(ts) : udata;
  /* insert it on table */
  newentry(L, &G(L)->udt, ts, lmod(IntPoint(ts->u.d.value), G(L)->udt.size));
  return ts;
}


int luaS_createudata (lua_State *L, void *udata, TObject *o) {
  int h1 = lmod(IntPoint(udata), G(L)->udt.size);
  TString *ts;
  for (ts = G(L)->udt.hash[h1]; ts; ts = ts->nexthash) {
    if (udata == ts->u.d.value) {
      setuvalue(o, ts);
      return 0;
    }
  }
  /* not found */
  setuvalue(o, luaS_newudata(L, 0, udata));
  return 1;
}

