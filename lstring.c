/*
** $Id: lstring.c,v 1.27 1999/11/10 15:39:35 roberto Exp roberto $
** String table (keeps all strings handled by Lua)
** See Copyright Notice in lua.h
*/


#include <string.h>

#define LUA_REENTRANT

#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "lua.h"



#define gcsizestring(L, l)	numblocks(L, 0, sizeof(TaggedString)+l)
#define gcsizeudata	gcsizestring(L, 0)



/*
** to avoid hash tables with size = 0 (cannot hash with size=0), all
** hash tables are initialized with this `array'. Elements are never
** written here, because this table (with size=1) must grow to get an
** element, and before it grows we replace it for a `real' table.
**
*/
static TaggedString *init_hash[1] = {NULL};


void luaS_init (lua_State *L) {
  int i;
  L->string_root = luaM_newvector(L, NUM_HASHS, stringtable);
  for (i=0; i<NUM_HASHS; i++) {
    L->string_root[i].size = 1;
    L->string_root[i].nuse = 0;
    L->string_root[i].hash = init_hash;
  }
}


void luaS_freeall (lua_State *L) {
  int i;
  for (i=0; i<NUM_HASHS; i++) {
    LUA_ASSERT(L, L->string_root[i].nuse==0, "non-empty string table");
    if (L->string_root[i].hash != init_hash)
      luaM_free(L, L->string_root[i].hash);
  }
  luaM_free(L, L->string_root);
  LUA_ASSERT(L, init_hash[0] == NULL, "init_hash corrupted");
}


static unsigned long hash_s (const char *s, long l) {
  unsigned long h = l;  /* seed */
  while (l--)
      h = h ^ ((h<<5)+(h>>2)+(unsigned char)*(s++));
  return h;
}


void luaS_grow (lua_State *L, stringtable *tb) {
  int ns = luaO_redimension(L, tb->nuse*2);  /* new size */
  TaggedString **newhash = luaM_newvector(L, ns, TaggedString *);
  int i;
  for (i=0; i<ns; i++) newhash[i] = NULL;
  /* rehash */
  for (i=0; i<tb->size; i++) {
    TaggedString *p = tb->hash[i];
    while (p) {  /* for each node in the list */
      TaggedString *next = p->nexthash;  /* save next */
      int h = p->hash%ns;  /* new position */
      p->nexthash = newhash[h];  /* chain it in new position */
      newhash[h] = p;
      p = next;
    }
  }
  luaM_free(L, tb->hash);
  tb->size = ns;
  tb->hash = newhash;
}


static TaggedString *newone (lua_State *L, long l, unsigned long h) {
  TaggedString *ts = (TaggedString *)luaM_malloc(L, 
                                       sizeof(TaggedString)+l*sizeof(char));
  ts->marked = 0;
  ts->nexthash = NULL;
  ts->hash = h;
  return ts;
}


static TaggedString *newone_s (lua_State *L, const char *str, long l, unsigned long h) {
  TaggedString *ts = newone(L, l, h);
  memcpy(ts->str, str, l);
  ts->str[l] = 0;  /* ending 0 */
  ts->u.s.gv = NULL;  /* no global value */
  ts->u.s.len = l;
  ts->constindex = 0;
  L->nblocks += gcsizestring(L, l);
  return ts;
}


static TaggedString *newone_u (lua_State *L, void *buff, int tag, unsigned long h) {
  TaggedString *ts = newone(L, 0, h);
  ts->u.d.value = buff;
  ts->u.d.tag = (tag == LUA_ANYTAG) ? 0 : tag;
  ts->constindex = -1;  /* tag -> this is a userdata */
  L->nblocks += gcsizeudata;
  return ts;
}


static void newentry (lua_State *L, stringtable *tb, TaggedString *ts, int h) {
  tb->nuse++;
  if (tb->nuse >= tb->size) {  /* no more room? */
    if (tb->hash == init_hash) {  /* cannot change init_hash */
      LUA_ASSERT(L, h==0, "`init_hash' has size 1");
      tb->hash = luaM_newvector(L, 1, TaggedString *);  /* so, `clone' it */
      tb->hash[0] = NULL;
    }
    luaS_grow(L, tb);
    h = ts->hash%tb->size;  /* new hash position */
  }
  ts->nexthash = tb->hash[h];  /* chain new entry */
  tb->hash[h] = ts;
}


TaggedString *luaS_newlstr (lua_State *L, const char *str, long l) {
  unsigned long h = hash_s(str, l);
  stringtable *tb = &L->string_root[h%NUM_HASHSTR];
  int h1 = h%tb->size;
  TaggedString *ts;
  for (ts = tb->hash[h1]; ts; ts = ts->nexthash) {
    if (ts->u.s.len == l && (memcmp(str, ts->str, l) == 0))
      return ts;
  }
  /* not found */
  ts = newone_s(L, str, l, h);  /* create new entry */
  newentry(L, tb, ts, h1);  /* insert it on table */
  return ts;
}


TaggedString *luaS_createudata (lua_State *L, void *udata, int tag) {
  unsigned long h = IntPoint(L, udata);
  stringtable *tb = &L->string_root[(h%NUM_HASHUDATA)+NUM_HASHSTR];
  int h1 = h%tb->size;
  TaggedString *ts;
  for (ts = tb->hash[h1]; ts; ts = ts->nexthash) {
    if (udata == ts->u.d.value && (tag == ts->u.d.tag || tag == LUA_ANYTAG))
      return ts;
  }
  /* not found */
  ts = newone_u(L, udata, tag, h);
  newentry(L, tb, ts, h1);
  return ts;
}


TaggedString *luaS_new (lua_State *L, const char *str) {
  return luaS_newlstr(L, str, strlen(str));
}

TaggedString *luaS_newfixedstring (lua_State *L, const char *str) {
  TaggedString *ts = luaS_new(L, str);
  if (ts->marked == 0) ts->marked = FIXMARK;  /* avoid GC */
  return ts;
}


void luaS_free (lua_State *L, TaggedString *t) {
  if (t->constindex == -1)  /* is userdata? */
    L->nblocks -= gcsizeudata;
  else {  /* is string */
    L->nblocks -= gcsizestring(L, t->u.s.len);
    luaM_free(L, t->u.s.gv);
  }
  luaM_free(L, t);
}


GlobalVar *luaS_assertglobal (lua_State *L, TaggedString *ts) {
  GlobalVar *gv = ts->u.s.gv;
  if (!gv) {  /* no global value yet? */
    gv = luaM_new(L, GlobalVar);
    gv->value.ttype = LUA_T_NIL;  /* initial value */
    gv->name = ts;
    gv->next = L->rootglobal;  /* chain in global list */
    L->rootglobal = gv; 
    ts->u.s.gv = gv;
  }
  return gv;
}


GlobalVar *luaS_assertglobalbyname (lua_State *L, const char *name) {
  return luaS_assertglobal(L, luaS_new(L, name));
}


int luaS_globaldefined (lua_State *L, const char *name) {
  TaggedString *ts = luaS_new(L, name);
  return ts->u.s.gv && ts->u.s.gv->value.ttype != LUA_T_NIL;
}


