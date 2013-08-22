/*
** $Id: lstring.c,v 2.29 2013/08/21 19:21:16 roberto Exp roberto $
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


/* mark for vacant places in hash table */
#define VACANTK		cast(TString *, cast(size_t, -1))


/* second hash (for double hash) */
#define h2(h1,hash,size)	lmod(h1 + ((hash % 61) | 1), size)


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


/*
** equality for strings
*/
int luaS_eqstr (TString *a, TString *b) {
  return (a->tsv.tt == b->tsv.tt) &&
         (a->tsv.tt == LUA_TSHRSTR ? eqshrstr(a, b) : luaS_eqlngstr(a, b));
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
  TString **oldhash = tb->hash;
  int oldsize = tb->size;
  tb->hash = luaM_newvector(L, newsize, TString *);
  tb->size = newsize;
  /* keep load factor below 75% */
  tb->empty = newsize/2 + newsize/4 - tb->nuse;
  for (i = 0; i < newsize; i++) tb->hash[i] = NULL;
  tb->nuse = 0;
  /* rehash */
  for (i = 0; i < oldsize; i++) {
    TString *ts = oldhash[i];
    if (ts != NULL && ts != VACANTK) {
      unsigned int hash = ts->tsv.hash;
      int h1 = lmod(hash, tb->size);
      while (tb->hash[h1] != NULL)
        h1 = h2(h1, hash, tb->size);
      tb->hash[h1] = ts;
      tb->nuse++;
    }
  }
  luaM_freearray(L, oldhash, oldsize);
}


/*
** creates a new string object
*/
static TString *createstrobj (lua_State *L, const char *str, size_t l,
                              int tag, unsigned int h) {
  TString *ts;
  size_t totalsize;  /* total size of TString object */
  totalsize = sizeof(TString) + ((l + 1) * sizeof(char));
  ts = &luaC_newobj(L, tag, totalsize, NULL, 0)->ts;
  ts->tsv.len = l;
  ts->tsv.hash = h;
  ts->tsv.extra = 0;
  memcpy(ts+1, str, l*sizeof(char));
  ((char *)(ts+1))[l] = '\0';  /* ending 0 */
  return ts;
}


static void rehash (lua_State *L, stringtable *tb) {
  int size = tb->size;
  if (tb->nuse < size / 2) {  /* using less than half the size? */
    if (tb->nuse < size / 4)  /* using less than half of that? */
      size /= 2;  /* shrink table */
    /* else keep size (but reorganize table) */
  }
  else {  /* table must grow */
    if (size >= MAX_INT/2)  /* avoid arith. overflow */
      luaD_throw(L, LUA_ERRMEM);  /* regular errors need new strings... */
    size *= 2;
  }
  luaS_resize(L, size);
}


LUAI_FUNC void luaS_remove (lua_State *L, TString *ts) {
  stringtable *tb = &G(L)->strt;
  unsigned int hash = ts->tsv.hash;
  int h1 = lmod(hash, tb->size);
  while (tb->hash[h1] != ts) {
    lua_assert(tb->hash[h1] != NULL);
    h1 = h2(h1, hash, tb->size);
  }
  tb->hash[h1] = VACANTK;
  tb->nuse--;
}


/*
** checks whether short string exists and reuses it or creates a new one
*/
static TString *internshrstr (lua_State *L, const char *str, size_t l) {
  TString *ts;
  unsigned int hash = luaS_hash(str, l, G(L)->seed);
  stringtable *tb = &G(L)->strt;
  int vacant = -1;
  int h1;
  h1 = lmod(hash, tb->size);  /* previous call can changed 'size' */
  while ((ts = tb->hash[h1]) != NULL) {  /* search the string in hash table */
    if (ts == VACANTK) {
      if (vacant < 0) vacant = h1;  /* keep track of first vacant place */
    }
    else if (l == ts->tsv.len &&
            (memcmp(str, getstr(ts), l * sizeof(char)) == 0)) {
      /* found! */
      if (isdead(G(L), obj2gco(ts)))  /* dead (but was not collected yet)? */
        changewhite(obj2gco(ts));  /* resurrect it */
      if (vacant >= 0) {  /* is there a better place for this string? */
        tb->hash[vacant] = ts;  /* move it up the line */
        tb->hash[h1] = VACANTK;
      }
      return ts;  /* found */
    }
    h1 = h2(h1, hash, tb->size);
  }
  if (tb->empty <= 0) {  /* no more empty spaces? */
    rehash(L, tb);
    return internshrstr(L, str, l);  /* recompute insertion with new size */
  }
  ts = createstrobj(L, str, l, LUA_TSHRSTR, hash);
  tb->nuse++;
  if (vacant < 0)  /* found no vacant place? */
    tb->empty--;  /* will have to use the empty place */
  else
    h1 = vacant;  /* use vacant place */
  tb->hash[h1] = ts;
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


Udata *luaS_newudata (lua_State *L, size_t s, Table *e) {
  Udata *u;
  if (s > MAX_SIZE - sizeof(Udata))
    luaM_toobig(L);
  u = &luaC_newobj(L, LUA_TUSERDATA, sizeof(Udata) + s, NULL, 0)->u;
  u->uv.len = s;
  u->uv.metatable = NULL;
  u->uv.env = e;
  return u;
}

