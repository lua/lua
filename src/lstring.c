/*
** $Id: lstring.c,v 1.13 1998/06/19 16:14:09 roberto Exp $
** String table (keeps all strings handled by Lua)
** See Copyright Notice in lua.h
*/


#include <string.h>

#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "lua.h"


#define NUM_HASHS  61


#define gcsizestring(l)	(1+(l/64))  /* "weight" for a string with length 'l' */



static TaggedString EMPTY = {{NULL, 2}, 0L, 0,
                            {{{LUA_T_NIL, {NULL}}, 0L}}, {0}};


void luaS_init (void)
{
  int i;
  L->string_root = luaM_newvector(NUM_HASHS, stringtable);
  for (i=0; i<NUM_HASHS; i++) {
    L->string_root[i].size = 0;
    L->string_root[i].nuse = 0;
    L->string_root[i].hash = NULL;
  }
}


static unsigned long hash_s (char *s, long l)
{
  unsigned long h = 0;
  while (l--)
    h = ((h<<5)-h)^(unsigned char)*(s++);
  return h;
}

static int newsize (stringtable *tb)
{
  int size = tb->size;
  int realuse = 0;
  int i;
  /* count how many entries are really in use */
  for (i=0; i<size; i++)
    if (tb->hash[i] != NULL && tb->hash[i] != &EMPTY)
      realuse++;
  if (2*(realuse+1) <= size)  /* +1 is the new element */
    return size;  /* don't need to grow, just rehash to clear EMPTYs */
  else
    return luaO_redimension(size);
}


static void grow (stringtable *tb)
{
  
  int ns = newsize(tb);
  TaggedString **newhash = luaM_newvector(ns, TaggedString *);
  int i;
  for (i=0; i<ns; i++)
    newhash[i] = NULL;
  /* rehash */
  tb->nuse = 0;
  for (i=0; i<tb->size; i++) {
    if (tb->hash[i] != NULL && tb->hash[i] != &EMPTY) {
      int h = tb->hash[i]->hash%ns;
      while (newhash[h])
        h = (h+1)%ns;
      newhash[h] = tb->hash[i];
      tb->nuse++;
    }
  }
  luaM_free(tb->hash);
  tb->size = ns;
  tb->hash = newhash;
}


static TaggedString *newone_s (char *str, long l, unsigned long h)
{
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

static TaggedString *newone_u (char *buff, int tag, unsigned long h)
{
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

static TaggedString *insert_s (char *str, long l, stringtable *tb)
{
  TaggedString *ts;
  unsigned long h = hash_s(str, l);
  int size = tb->size;
  int i;
  int j = -1;
  if ((long)tb->nuse*3 >= (long)size*2) {
    grow(tb);
    size = tb->size;
  }
  for (i = h%size; (ts = tb->hash[i]) != NULL; ) {
    if (ts == &EMPTY)
      j = i;
    else if (ts->constindex >= 0 &&
             ts->u.s.len == l &&
             (memcmp(str, ts->str, l) == 0))
      return ts;
    if (++i == size) i=0;
  }
  /* not found */
  if (j != -1)  /* is there an EMPTY space? */
    i = j;
  else
    tb->nuse++;
  ts = tb->hash[i] = newone_s(str, l, h);
  return ts;
}

static TaggedString *insert_u (void *buff, int tag, stringtable *tb)
{
  TaggedString *ts;
  unsigned long h = (unsigned long)buff;
  int size = tb->size;
  int i;
  int j = -1;
  if ((long)tb->nuse*3 >= (long)size*2) {
    grow(tb);
    size = tb->size;
  }
  for (i = h%size; (ts = tb->hash[i]) != NULL; ) {
    if (ts == &EMPTY)
      j = i;
    else if (ts->constindex < 0 &&  /* is a udata? */
             (tag == ts->u.d.tag || tag == LUA_ANYTAG) &&
             buff == ts->u.d.v)
      return ts;
    if (++i == size) i=0;
  }
  /* not found */
  if (j != -1)  /* is there an EMPTY space? */
    i = j;
  else
    tb->nuse++;
  ts = tb->hash[i] = newone_u(buff, tag, h);
  return ts;
}

TaggedString *luaS_createudata (void *udata, int tag)
{
  return insert_u(udata, tag, &L->string_root[(unsigned)udata%NUM_HASHS]);
}

TaggedString *luaS_newlstr (char *str, long l)
{
  int i = (l==0)?0:(unsigned char)str[0];
  return insert_s(str, l, &L->string_root[i%NUM_HASHS]);
}

TaggedString *luaS_new (char *str)
{
  return luaS_newlstr(str, strlen(str));
}

TaggedString *luaS_newfixedstring (char *str)
{
  TaggedString *ts = luaS_new(str);
  if (ts->head.marked == 0)
    ts->head.marked = 2;  /* avoid GC */
  return ts;
}


void luaS_free (TaggedString *l)
{
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

static void remove_from_list (GCnode *l)
{
  while (l) {
    GCnode *next = l->next;
    while (next && !next->marked)
      next = l->next = next->next;
    l = next;
  }
}


TaggedString *luaS_collector (void)
{
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


TaggedString *luaS_collectudata (void)
{
  TaggedString *frees = NULL;
  int i;
  L->rootglobal.next = NULL;  /* empty list of globals */
  for (i=0; i<NUM_HASHS; i++) {
    stringtable *tb = &L->string_root[i];
    int j;
    for (j=0; j<tb->size; j++) {
      TaggedString *t = tb->hash[j];
      if (t == NULL || t == &EMPTY || t->constindex != -1)
        continue;  /* get only user data */
      t->head.next = (GCnode *)frees;
      frees = t;
      tb->hash[j] = &EMPTY;
    }
  }
  return frees;
}


void luaS_freeall (void)
{
  int i;
  for (i=0; i<NUM_HASHS; i++) {
    stringtable *tb = &L->string_root[i];
    int j;
    for (j=0; j<tb->size; j++) {
      TaggedString *t = tb->hash[j];
      if (t == &EMPTY) continue;
      luaM_free(t);
    }
    luaM_free(tb->hash);
  }
  luaM_free(L->string_root);
}


void luaS_rawsetglobal (TaggedString *ts, TObject *newval)
{
  ts->u.s.globalval = *newval;
  if (ts->head.next == (GCnode *)ts) {  /* is not in list? */
    ts->head.next = L->rootglobal.next;
    L->rootglobal.next = (GCnode *)ts;
  }
}


char *luaS_travsymbol (int (*fn)(TObject *))
{
  TaggedString *g;
  for (g=(TaggedString *)L->rootglobal.next; g; g=(TaggedString *)g->head.next)
    if (fn(&g->u.s.globalval))
      return g->str;
  return NULL;
}


int luaS_globaldefined (char *name)
{
  TaggedString *ts = luaS_new(name);
  return ts->u.s.globalval.ttype != LUA_T_NIL;
}

