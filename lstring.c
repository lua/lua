/*
** $Id: lstring.c,v 1.9 1997/12/30 19:15:52 roberto Exp roberto $
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



static TaggedString EMPTY = {{NULL, 2}, 0, 0L, {{LUA_T_NIL, {NULL}}}, {0}};


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


static unsigned long hash (char *s, int tag)
{
  unsigned long h;
  if (tag != LUA_T_STRING)
    h = (unsigned long)s;
  else {
    h = 0;
    while (*s)
      h = ((h<<5)-h)^(unsigned char)*(s++);
  }
  return h;
}


static void grow (stringtable *tb)
{
  int newsize = luaO_redimension(tb->size);
  TaggedString **newhash = luaM_newvector(newsize, TaggedString *);
  int i;
  for (i=0; i<newsize; i++)
    newhash[i] = NULL;
  /* rehash */
  tb->nuse = 0;
  for (i=0; i<tb->size; i++) {
    if (tb->hash[i] != NULL && tb->hash[i] != &EMPTY) {
      int h = tb->hash[i]->hash%newsize;
      while (newhash[h])
        h = (h+1)%newsize;
      newhash[h] = tb->hash[i];
      tb->nuse++;
    }
  }
  luaM_free(tb->hash);
  tb->size = newsize;
  tb->hash = newhash;
}


static TaggedString *newone (char *buff, int tag, unsigned long h)
{
  TaggedString *ts;
  if (tag == LUA_T_STRING) {
    long l = strlen(buff);
    ts = (TaggedString *)luaM_malloc(sizeof(TaggedString)+l);
    strcpy(ts->str, buff);
    ts->u.globalval.ttype = LUA_T_NIL;  /* initialize global value */
    ts->constindex = 0;
    L->nblocks += gcsizestring(l);
  }
  else {
    ts = (TaggedString *)luaM_malloc(sizeof(TaggedString));
    ts->u.d.v = buff;
    ts->u.d.tag = tag == LUA_ANYTAG ? 0 : tag;
    ts->constindex = -1;  /* tag -> this is a userdata */
    L->nblocks++;
  }
  ts->head.marked = 0;
  ts->head.next = (GCnode *)ts;  /* signal it is in no list */
  ts->hash = h;
  return ts;
}

static TaggedString *insert (char *buff, int tag, stringtable *tb)
{
  TaggedString *ts;
  unsigned long h = hash(buff, tag);
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
    else if ((ts->constindex >= 0) ?  /* is a string? */
              (tag == LUA_T_STRING && (strcmp(buff, ts->str) == 0)) :
              ((tag == ts->u.d.tag || tag == LUA_ANYTAG) && buff == ts->u.d.v))
      return ts;
    if (++i == size) i=0;
  }
  /* not found */
  if (j != -1)  /* is there an EMPTY space? */
    i = j;
  else
    tb->nuse++;
  ts = tb->hash[i] = newone(buff, tag, h);
  return ts;
}

TaggedString *luaS_createudata (void *udata, int tag)
{
  return insert(udata, tag, &L->string_root[(unsigned)udata%NUM_HASHS]);
}

TaggedString *luaS_new (char *str)
{
  return insert(str, LUA_T_STRING, &L->string_root[(unsigned)str[0]%NUM_HASHS]);
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
    L->nblocks -= (l->constindex == -1) ? 1 : gcsizestring(strlen(l->str));
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
        continue;  /* get only user datas */
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
  ts->u.globalval = *newval;
  if (ts->head.next == (GCnode *)ts) {  /* is not in list? */
    ts->head.next = L->rootglobal.next;
    L->rootglobal.next = (GCnode *)ts;
  }
}


char *luaS_travsymbol (int (*fn)(TObject *))
{
  TaggedString *g;
  for (g=(TaggedString *)L->rootglobal.next; g; g=(TaggedString *)g->head.next)
    if (fn(&g->u.globalval))
      return g->str;
  return NULL;
}


int luaS_globaldefined (char *name)
{
  TaggedString *ts = luaS_new(name);
  return ts->u.globalval.ttype != LUA_T_NIL;
}

