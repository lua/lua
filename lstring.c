/*
** $Id: lstring.c,v 1.1 1997/08/14 20:23:30 roberto Exp $
** String table (keep all strings handled by Lua)
** See Copyright Notice in lua.h
*/


#include <string.h>

#include "lmem.h"
#include "lobject.h"
#include "lstring.h"
#include "lua.h"


#define NUM_HASHS  61

typedef struct {
  int size;
  int nuse;  /* number of elements (including EMPTYs) */
  TaggedString **hash;
} stringtable;


static stringtable string_root[NUM_HASHS] = {
{0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL},
{0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL},
{0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL},
{0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL},
{0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL},
{0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL},
{0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL},
{0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL},
{0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL},
{0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL},
{0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL},
{0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL}, {0, 0, NULL},
{0, 0, NULL}
};


static TaggedString EMPTY = {LUA_T_STRING, {0}, {{NOT_USED, NOT_USED}}, 2, {0}};



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
      int h = tb->hash[i]->uu.hash%newsize;
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


static TaggedString *newone(char *buff, int tag, unsigned long h)
{
  TaggedString *ts;
  if (tag == LUA_T_STRING) {
    ts = (TaggedString *)luaM_malloc(sizeof(TaggedString)+strlen(buff));
    strcpy(ts->str, buff);
    ts->u.s.varindex = ts->u.s.constindex = NOT_USED;
    ts->tag = LUA_T_STRING;
  }
  else {
    ts = (TaggedString *)luaM_malloc(sizeof(TaggedString));
    ts->u.v = buff;
    ts->tag = tag == LUA_ANYTAG ? 0 : tag;
  }
  ts->marked = 0;
  ts->uu.hash = h;
  return ts;
}

static TaggedString *insert (char *buff, int tag, stringtable *tb)
{
  TaggedString *ts;
  unsigned long h = hash(buff, tag);
  int i;
  int j = -1;
  if ((long)tb->nuse*3 >= (long)tb->size*2)
    grow(tb);
  i = h%tb->size;
  while ((ts = tb->hash[i]) != NULL)
  {
    if (ts == &EMPTY)
      j = i;
    else if ((ts->tag == LUA_T_STRING) ?
              (tag == LUA_T_STRING && (strcmp(buff, ts->str) == 0)) :
              ((tag == ts->tag || tag == LUA_ANYTAG) && buff == ts->u.v))
      return ts;
    i = (i+1)%tb->size;
  }
  /* not found */
  ++luaO_nentities;
  if (j != -1)  /* is there an EMPTY space? */
    i = j;
  else
    tb->nuse++;
  ts = tb->hash[i] = newone(buff, tag, h);
  return ts;
}

TaggedString *luaS_createudata (void *udata, int tag)
{
  return insert(udata, tag, &string_root[(unsigned)udata%NUM_HASHS]);
}

TaggedString *luaS_new (char *str)
{
  return insert(str, LUA_T_STRING, &string_root[(unsigned)str[0]%NUM_HASHS]);
}

TaggedString *luaS_newfixedstring (char *str)
{
  TaggedString *ts = luaS_new(str);
  if (ts->marked == 0)
    ts->marked = 2;  /* avoid GC */
  return ts;
}


void luaS_free (TaggedString *l)
{
  while (l) {
    TaggedString *next = l->uu.next;
    luaM_free(l);
    l = next;
  }
}


/*
** Garbage collection function.
*/
TaggedString *luaS_collector (void)
{
  TaggedString *frees = NULL;
  int i;
  for (i=0; i<NUM_HASHS; i++) {
    stringtable *tb = &string_root[i];
    int j;
    for (j=0; j<tb->size; j++) {
      TaggedString *t = tb->hash[j];
      if (t == NULL) continue;
      if (t->marked == 1)
        t->marked = 0;
      else if (!t->marked) {
        t->uu.next = frees;
        frees = t;
        tb->hash[j] = &EMPTY;
        --luaO_nentities;
      }
    }
  }
  return frees;
}

