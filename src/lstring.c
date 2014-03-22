/*
** $Id: lstring.c,v 2.38 2014/03/19 18:51:42 roberto Exp $
** String table (keeps all strings handled by Lua)
** See Copyright Notice in lua.h
*/


#include <string.h>

#define lstring_c
#define LUA_CORE

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"



/*
** Lua will use at most ~(2^LUAI_HASHLIMIT) bytes from a string to
** compute its hash
*/
#if !defined(LUAI_HASHLIMIT)
#define LUAI_HASHLIMIT		5
#endif


/*
** equality for long strings
*/
int luaS_eqlngstr (TString *a, TString *b) {
  size_t len = a->tsv.len;
  lua_assert(a->tsv.tt == LUA_TLNGSTR && b->tsv.tt == LUA_TLNGSTR);
  return (a == b) ||  /* same instance or... */
    ((len == b->tsv.len) &&  /* equal length and ... */
     (memcmp(getstr(a), getstr(b), len) == 0));  /* equal contents */
}


unsigned int luaS_hash (const char *str, size_t l, unsigned int seed) {
  unsigned int h = seed ^ cast(unsigned int, l);
  size_t l1;
  size_t step = (l >> LUAI_HASHLIMIT) + 1;
  for (l1 = l; l1 >= step; l1 -= step)
    h = h ^ ((h<<5) + (h>>2) + cast_byte(str[l1 - 1]));
  return h;
}


/*
** resizes the string table
*/
void luaS_resize (lua_State *L, int newsize) {
  int i;
  stringtable *tb = &G(L)->strt;
  if (newsize > tb->size) {  /* grow table if needed */
    luaM_reallocvector(L, tb->hash, tb->size, newsize, TString *);
    for (i = tb->size; i < newsize; i++)
      tb->hash[i] = NULL;
  }
  for (i = 0; i < tb->size; i++) {  /* rehash */
    TString *p = tb->hash[i];
    tb->hash[i] = NULL;
    while (p) {  /* for each node in the list */
      TString *hnext = p->tsv.hnext;  /* save next */
      unsigned int h = lmod(p->tsv.hash, newsize);  /* new position */
      p->tsv.hnext = tb->hash[h];  /* chain it */
      tb->hash[h] = p;
      p = hnext;
    }
  }
  if (newsize < tb->size) {  /* shrink table if needed */
    /* vanishing slice should be empty */
    lua_assert(tb->hash[newsize] == NULL && tb->hash[tb->size - 1] == NULL);
    luaM_reallocvector(L, tb->hash, tb->size, newsize, TString *);
  }
  tb->size = newsize;
}



/*
** creates a new string object
*/
static TString *createstrobj (lua_State *L, const char *str, size_t l,
                              int tag, unsigned int h) {
  TString *ts;
  size_t totalsize;  /* total size of TString object */
  totalsize = sizeof(TString) + ((l + 1) * sizeof(char));
  ts = &luaC_newobj(L, tag, totalsize)->ts;
  ts->tsv.len = l;
  ts->tsv.hash = h;
  ts->tsv.extra = 0;
  memcpy(ts+1, str, l*sizeof(char));
  ((char *)(ts+1))[l] = '\0';  /* ending 0 */
  return ts;
}


LUAI_FUNC void luaS_remove (lua_State *L, TString *ts) {
  stringtable *tb = &G(L)->strt;
  TString **p = &tb->hash[lmod(ts->tsv.hash, tb->size)];
  while (*p != ts)  /* find previous element */
    p = &(*p)->tsv.hnext;
  *p = (*p)->tsv.hnext;  /* remove element from its list */
  tb->nuse--;
}


/*
** checks whether short string exists and reuses it or creates a new one
*/
static TString *internshrstr (lua_State *L, const char *str, size_t l) {
  TString *ts;
  global_State *g = G(L);
  unsigned int h = luaS_hash(str, l, g->seed);
  TString **list = &g->strt.hash[lmod(h, g->strt.size)];
  for (ts = *list; ts != NULL; ts = ts->tsv.hnext) {
    if (l == ts->tsv.len &&
        (memcmp(str, getstr(ts), l * sizeof(char)) == 0)) {
      /* found! */
      if (isdead(g, obj2gco(ts)))  /* dead (but not collected yet)? */
        changewhite(obj2gco(ts));  /* resurrect it */
      return ts;
    }
  }
  if (g->strt.nuse >= g->strt.size && g->strt.size <= MAX_INT/2) {
    luaS_resize(L, g->strt.size * 2);
    list = &g->strt.hash[lmod(h, g->strt.size)];  /* recompute with new size */
  }
  ts = createstrobj(L, str, l, LUA_TSHRSTR, h);
  ts->tsv.hnext = *list;
  *list = ts;
  g->strt.nuse++;
  return ts;
}


/*
** new string (with explicit length)
*/
TString *luaS_newlstr (lua_State *L, const char *str, size_t l) {
  if (l <= LUAI_MAXSHORTLEN)  /* short string? */
    return internshrstr(L, str, l);
  else {
    if (l + 1 > (MAX_SIZE - sizeof(TString))/sizeof(char))
      luaM_toobig(L);
    return createstrobj(L, str, l, LUA_TLNGSTR, G(L)->seed);
  }
}


/*
** new zero-terminated string
*/
TString *luaS_new (lua_State *L, const char *str) {
  return luaS_newlstr(L, str, strlen(str));
}


Udata *luaS_newudata (lua_State *L, size_t s) {
  Udata *u;
  if (s > MAX_SIZE - sizeof(Udata))
    luaM_toobig(L);
  u = &luaC_newobj(L, LUA_TUSERDATA, sizeof(Udata) + s)->u;
  u->uv.len = s;
  u->uv.metatable = NULL;
  setuservalue(L, u, luaO_nilobject);
  return u;
}

