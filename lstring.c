/*
** $Id: lstring.c,v 1.26 1999/11/04 17:22:26 roberto Exp roberto $
** String table (keeps all strings handled by Lua)
** See Copyright Notice in lua.h
*/


#include <string.h>

#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "lua.h"



#define gcsizestring(l)	numblocks(0, sizeof(TaggedString)+l)
#define gcsizeudata	gcsizestring(0)



/*
** to avoid hash tables with size = 0 (cannot hash with size=0), all
** hash tables are initialized with this `array'. Elements are never
** written here, because this table (with size=1) must grow to get an
** element, and before it grows we replace it for a `real' table.
**
*/
static TaggedString *init_hash[1] = {NULL};


void luaS_init (void) {
  int i;
  L->string_root = luaM_newvector(NUM_HASHS, stringtable);
  for (i=0; i<NUM_HASHS; i++) {
    L->string_root[i].size = 1;
    L->string_root[i].nuse = 0;
    L->string_root[i].hash = init_hash;
  }
}


void luaS_freeall (void) {
  int i;
  for (i=0; i<NUM_HASHS; i++) {
    LUA_ASSERT(L->string_root[i].nuse==0, "non-empty string table");
    if (L->string_root[i].hash != init_hash)
      luaM_free(L->string_root[i].hash);
  }
  luaM_free(L->string_root);
  LUA_ASSERT(init_hash[0] == NULL, "init_hash corrupted");
}


static unsigned long hash_s (const char *s, long l) {
  unsigned long h = l;  /* seed */
  while (l--)
      h = h ^ ((h<<5)+(h>>2)+(unsigned char)*(s++));
  return h;
}


void luaS_grow (stringtable *tb) {
  int ns = luaO_redimension(tb->nuse*2);  /* new size */
  TaggedString **newhash = luaM_newvector(ns, TaggedString *);
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
  luaM_free(tb->hash);
  tb->size = ns;
  tb->hash = newhash;
}


static TaggedString *newone (long l, unsigned long h) {
  TaggedString *ts = (TaggedString *)luaM_malloc(
                                       sizeof(TaggedString)+l*sizeof(char));
  ts->marked = 0;
  ts->nexthash = NULL;
  ts->hash = h;
  return ts;
}


static TaggedString *newone_s (const char *str, long l, unsigned long h) {
  TaggedString *ts = newone(l, h);
  memcpy(ts->str, str, l);
  ts->str[l] = 0;  /* ending 0 */
  ts->u.s.gv = NULL;  /* no global value */
  ts->u.s.len = l;
  ts->constindex = 0;
  L->nblocks += gcsizestring(l);
  return ts;
}


static TaggedString *newone_u (void *buff, int tag, unsigned long h) {
  TaggedString *ts = newone(0, h);
  ts->u.d.value = buff;
  ts->u.d.tag = (tag == LUA_ANYTAG) ? 0 : tag;
  ts->constindex = -1;  /* tag -> this is a userdata */
  L->nblocks += gcsizeudata;
  return ts;
}


static void newentry (stringtable *tb, TaggedString *ts, int h) {
  tb->nuse++;
  if (tb->nuse >= tb->size) {  /* no more room? */
    if (tb->hash == init_hash) {  /* cannot change init_hash */
      LUA_ASSERT(h==0, "`init_hash' has size 1");
      tb->hash = luaM_newvector(1, TaggedString *);  /* so, `clone' it */
      tb->hash[0] = NULL;
    }
    luaS_grow(tb);
    h = ts->hash%tb->size;  /* new hash position */
  }
  ts->nexthash = tb->hash[h];  /* chain new entry */
  tb->hash[h] = ts;
}


TaggedString *luaS_newlstr (const char *str, long l) {
  unsigned long h = hash_s(str, l);
  stringtable *tb = &L->string_root[h%NUM_HASHSTR];
  int h1 = h%tb->size;
  TaggedString *ts;
  for (ts = tb->hash[h1]; ts; ts = ts->nexthash) {
    if (ts->u.s.len == l && (memcmp(str, ts->str, l) == 0))
      return ts;
  }
  /* not found */
  ts = newone_s(str, l, h);  /* create new entry */
  newentry(tb, ts, h1);  /* insert it on table */
  return ts;
}


TaggedString *luaS_createudata (void *udata, int tag) {
  unsigned long h = IntPoint(udata);
  stringtable *tb = &L->string_root[(h%NUM_HASHUDATA)+NUM_HASHSTR];
  int h1 = h%tb->size;
  TaggedString *ts;
  for (ts = tb->hash[h1]; ts; ts = ts->nexthash) {
    if (udata == ts->u.d.value && (tag == ts->u.d.tag || tag == LUA_ANYTAG))
      return ts;
  }
  /* not found */
  ts = newone_u(udata, tag, h);
  newentry(tb, ts, h1);
  return ts;
}


TaggedString *luaS_new (const char *str) {
  return luaS_newlstr(str, strlen(str));
}

TaggedString *luaS_newfixedstring (const char *str) {
  TaggedString *ts = luaS_new(str);
  if (ts->marked == 0) ts->marked = FIXMARK;  /* avoid GC */
  return ts;
}


void luaS_free (TaggedString *t) {
  if (t->constindex == -1)  /* is userdata? */
    L->nblocks -= gcsizeudata;
  else {  /* is string */
    L->nblocks -= gcsizestring(t->u.s.len);
    luaM_free(t->u.s.gv);
  }
  luaM_free(t);
}


GlobalVar *luaS_assertglobal (TaggedString *ts) {
  GlobalVar *gv = ts->u.s.gv;
  if (!gv) {  /* no global value yet? */
    gv = luaM_new(GlobalVar);
    gv->value.ttype = LUA_T_NIL;  /* initial value */
    gv->name = ts;
    gv->next = L->rootglobal;  /* chain in global list */
    L->rootglobal = gv; 
    ts->u.s.gv = gv;
  }
  return gv;
}


GlobalVar *luaS_assertglobalbyname (const char *name) {
  return luaS_assertglobal(luaS_new(name));
}


int luaS_globaldefined (const char *name) {
  TaggedString *ts = luaS_new(name);
  return ts->u.s.gv && ts->u.s.gv->value.ttype != LUA_T_NIL;
}


