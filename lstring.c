/*
** $Id: lstring.c,v 1.20 1999/08/16 20:52:00 roberto Exp roberto $
** String table (keeps all strings handled by Lua)
** See Copyright Notice in lua.h
*/


#include <string.h>

#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "lua.h"


#define NUM_HASHSTR	31
#define NUM_HASHUDATA	31
#define NUM_HASHS  (NUM_HASHSTR+NUM_HASHUDATA)


#define gcsizestring(l)	(1+(l/64))  /* "weight" for a string with length 'l' */



static TaggedString EMPTY = {{NULL, 2}, 0L, 0,
                            {{{LUA_T_NIL, {NULL}}, 0L}}, {0}};



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


static unsigned long hash_s (const char *s, long l) {
  unsigned long h = 0;  /* seed */
  while (l--)
      h = h ^ ((h<<5)+(h>>2)+(unsigned char)*(s++));
  return h;
}


static int newsize (stringtable *tb) {
  int realuse = 0;
  int i;
  /* count how many entries are really in use */
  for (i=0; i<tb->size; i++) {
    if (tb->hash[i] != NULL && tb->hash[i] != &EMPTY)
      realuse++;
  }
  return luaO_redimension(realuse*2);
}


static void grow (stringtable *tb) {
  int ns = newsize(tb);
  TaggedString **newhash = luaM_newvector(ns, TaggedString *);
  int i;
  for (i=0; i<ns; i++)
    newhash[i] = NULL;
  /* rehash */
  tb->nuse = 0;
  for (i=0; i<tb->size; i++) {
    if (tb->hash[i] != NULL && tb->hash[i] != &EMPTY) {
      unsigned long h = tb->hash[i]->hash;
      int h1 = h%ns;
      while (newhash[h1]) {
        h1 += (h&(ns-2)) + 1;  /* double hashing */
        if (h1 >= ns) h1 -= ns;
      }
      newhash[h1] = tb->hash[i];
      tb->nuse++;
    }
  }
  luaM_free(tb->hash);
  tb->size = ns;
  tb->hash = newhash;
}


static TaggedString *newone_s (const char *str, long l, unsigned long h) {
  TaggedString *ts = (TaggedString *)luaM_malloc(sizeof(TaggedString)+l);
  memcpy(ts->str, str, l);
  ts->str[l] = 0;  /* ending 0 */
  ts->u.s.globalval.ttype = LUA_T_NIL;  /* initialize global value */
  ts->u.s.len = l;
  ts->constindex = 0;
  L->nblocks += gcsizestring(l);
  ts->head.marked = 0;
  ts->head.next = (GCnode *)ts;  /* signal it is in no list */
  ts->hash = h;
  return ts;
}

static TaggedString *newone_u (void *buff, int tag, unsigned long h) {
  TaggedString *ts = luaM_new(TaggedString);
  ts->u.d.v = buff;
  ts->u.d.tag = (tag == LUA_ANYTAG) ? 0 : tag;
  ts->constindex = -1;  /* tag -> this is a userdata */
  L->nblocks++;
  ts->head.marked = 0;
  ts->head.next = (GCnode *)ts;  /* signal it is in no list */
  ts->hash = h;
  return ts;
}


static void newentry (stringtable *tb, TaggedString *ts, int h1) {
  tb->nuse++;
  if ((long)tb->nuse*3 < (long)tb->size*2)  /* still have room? */
    tb->hash[h1] = ts;
  else {  /* must grow */
    if (tb->hash == init_hash) {  /* cannot change init_hash */
      LUA_ASSERT(h1==0, "`init_hash' has size 1");
      tb->hash = luaM_newvector(1, TaggedString *);  /* so, `clone' it */
    }
    tb->hash[h1] = ts;
    grow(tb);
  }
}


static TaggedString *insert_s (const char *str, long l, stringtable *tb) {
  TaggedString *ts;
  unsigned long h = hash_s(str, l);
  int size = tb->size;
  int j = -1;  /* last empty place found (or -1) */
  int h1 = h%size;
  while ((ts = tb->hash[h1]) != NULL) {
    if (ts == &EMPTY)
      j = h1;
    else if (ts->u.s.len == l && (memcmp(str, ts->str, l) == 0))
      return ts;
    h1 += (h&(size-2)) + 1;  /* double hashing */
    if (h1 >= size) h1 -= size;
  }
  /* not found */
  ts = newone_s(str, l, h);  /* create new entry */
  if (j != -1)  /* is there an EMPTY space? */
    tb->hash[j] = ts;  /* use it for new element */
  else
    newentry(tb, ts, h1);  /* no EMPTY places; must use a virgin one */
  return ts;
}


static TaggedString *insert_u (void *buff, int tag, stringtable *tb) {
  TaggedString *ts;
  unsigned long h = (unsigned long)buff;
  int size = tb->size;
  int j = -1;
  int h1 = h%size;
  while ((ts = tb->hash[h1]) != NULL) {
    if (ts == &EMPTY)
      j = h1;
    else if ((tag == ts->u.d.tag || tag == LUA_ANYTAG) && buff == ts->u.d.v)
      return ts;
    h1 += (h&(size-2)) + 1;
    if (h1 >= size) h1 -= size;
  }
  ts = newone_u(buff, tag, h);
  if (j != -1)
    tb->hash[j] = ts;
  else
    newentry(tb, ts, h1);
  return ts;
}


TaggedString *luaS_createudata (void *udata, int tag) {
  int t = ((unsigned int)udata%NUM_HASHUDATA)+NUM_HASHSTR;
  return insert_u(udata, tag, &L->string_root[t]);
}

TaggedString *luaS_newlstr (const char *str, long l) {
  int t = (l==0) ? 0 : ((int)((unsigned char)str[0]+l))%NUM_HASHSTR;
  return insert_s(str, l, &L->string_root[t]);
}

TaggedString *luaS_new (const char *str) {
  return luaS_newlstr(str, strlen(str));
}

TaggedString *luaS_newfixedstring (const char *str) {
  TaggedString *ts = luaS_new(str);
  if (ts->head.marked == 0)
    ts->head.marked = 2;  /* avoid GC */
  return ts;
}


void luaS_free (TaggedString *l) {
  while (l) {
    TaggedString *next = (TaggedString *)l->head.next;
    L->nblocks -= (l->constindex == -1) ? 1 : gcsizestring(l->u.s.len);
    luaM_free(l);
    l = next;
  }
}


/*
** Garbage collection functions.
*/

static void remove_from_list (GCnode *l) {
  while (l) {
    GCnode *next = l->next;
    while (next && !next->marked)
      next = l->next = next->next;
    l = next;
  }
}


TaggedString *luaS_collector (void) {
  TaggedString *frees = NULL;
  int i;
  remove_from_list(&(L->rootglobal));
  for (i=0; i<NUM_HASHS; i++) {
    stringtable *tb = &L->string_root[i];
    int j;
    for (j=0; j<tb->size; j++) {
      TaggedString *t = tb->hash[j];
      if (t == NULL) continue;
      if (t->head.marked == 1)
        t->head.marked = 0;
      else if (!t->head.marked) {
        t->head.next = (GCnode *)frees;
        frees = t;
        tb->hash[j] = &EMPTY;
      }
    }
  }
  return frees;
}


TaggedString *luaS_collectudata (void) {
  TaggedString *frees = NULL;
  int i;
  L->rootglobal.next = NULL;  /* empty list of globals */
  for (i=NUM_HASHSTR; i<NUM_HASHS; i++) {
    stringtable *tb = &L->string_root[i];
    int j;
    for (j=0; j<tb->size; j++) {
      TaggedString *t = tb->hash[j];
      if (t == NULL || t == &EMPTY)
        continue;
      LUA_ASSERT(t->constindex == -1, "must be userdata");
      t->head.next = (GCnode *)frees;
      frees = t;
      tb->hash[j] = &EMPTY;
    }
  }
  return frees;
}


void luaS_freeall (void) {
  int i;
  for (i=0; i<NUM_HASHS; i++) {
    stringtable *tb = &L->string_root[i];
    int j;
    for (j=0; j<tb->size; j++) {
      TaggedString *t = tb->hash[j];
      if (t != &EMPTY) luaM_free(t);
    }
    if (tb->hash != init_hash) luaM_free(tb->hash);
  }
  luaM_free(L->string_root);
}


void luaS_rawsetglobal (TaggedString *ts, TObject *newval) {
  ts->u.s.globalval = *newval;
  if (ts->head.next == (GCnode *)ts) {  /* is not in list? */
    ts->head.next = L->rootglobal.next;
    L->rootglobal.next = (GCnode *)ts;
  }
}


const char *luaS_travsymbol (int (*fn)(TObject *)) {
  TaggedString *g;
  for (g=(TaggedString *)L->rootglobal.next; g; g=(TaggedString *)g->head.next)
    if (fn(&g->u.s.globalval))
      return g->str;
  return NULL;
}


int luaS_globaldefined (const char *name) {
  TaggedString *ts = luaS_new(name);
  return ts->u.s.globalval.ttype != LUA_T_NIL;
}

