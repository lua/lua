/*
** tree.c
** TecCGraf - PUC-Rio
*/
 
char *rcs_tree="$Id: tree.c,v 1.28 1997/06/11 14:24:40 roberto Exp $";


#include <string.h>

#include "luamem.h"
#include "lua.h"
#include "tree.h"
#include "lex.h"
#include "hash.h"
#include "table.h"
#include "fallback.h"


#define NUM_HASHS  64

typedef struct {
  int size;
  int nuse;  /* number of elements (including EMPTYs) */
  TaggedString **hash;
} stringtable;

static int initialized = 0;

static stringtable string_root[NUM_HASHS];

static TaggedString EMPTY = {LUA_T_STRING, NULL, {{NOT_USED, NOT_USED}},
                             0, 2, {0}};


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


static void luaI_inittree (void)
{
  int i;
  for (i=0; i<NUM_HASHS; i++) {
    string_root[i].size = 0;
    string_root[i].nuse = 0;
    string_root[i].hash = NULL;
  }
}


static void initialize (void)
{
  initialized = 1;
  luaI_inittree();
  luaI_addReserved();
  luaI_initsymbol();
  luaI_initconstant();
  luaI_initfallbacks();
}


static void grow (stringtable *tb)
{
  int newsize = luaI_redimension(tb->size);
  TaggedString **newhash = newvector(newsize, TaggedString *);
  int i;
  for (i=0; i<newsize; i++)
    newhash[i] = NULL;
  /* rehash */
  tb->nuse = 0;
  for (i=0; i<tb->size; i++)
    if (tb->hash[i] != NULL && tb->hash[i] != &EMPTY) {
      int h = tb->hash[i]->hash%newsize;
      while (newhash[h])
        h = (h+1)%newsize;
      newhash[h] = tb->hash[i];
      tb->nuse++;
    }
  luaI_free(tb->hash); 
  tb->size = newsize;
  tb->hash = newhash;
}


static TaggedString *newone(char *buff, int tag, unsigned long h)
{
  TaggedString *ts;
  if (tag == LUA_T_STRING) {
    ts = (TaggedString *)luaI_malloc(sizeof(TaggedString)+strlen(buff));
    strcpy(ts->str, buff);
    ts->u.s.varindex = ts->u.s.constindex = NOT_USED;
    ts->tag = LUA_T_STRING;
  }
  else {
    ts = (TaggedString *)luaI_malloc(sizeof(TaggedString));
    ts->u.v = buff;
    ts->tag = tag == LUA_ANYTAG ? 0 : tag;
  }
  ts->marked = 0;
  ts->hash = h;
  return ts;
}

static TaggedString *insert (char *buff, int tag, stringtable *tb)
{
  TaggedString *ts;
  unsigned long h = hash(buff, tag);
  int i;
  int j = -1;
  if ((Long)tb->nuse*3 >= (Long)tb->size*2)
  {
    if (!initialized)
      initialize();
    grow(tb);
  }
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
  lua_pack();
  if (j != -1)  /* is there an EMPTY space? */
    i = j;
  else
    tb->nuse++;
  ts = tb->hash[i] = newone(buff, tag, h);
  return ts;
}

TaggedString *luaI_createudata (void *udata, int tag)
{
  return insert(udata, tag, &string_root[(unsigned)udata%NUM_HASHS]);
}

TaggedString *lua_createstring (char *str)
{
  return insert(str, LUA_T_STRING, &string_root[(unsigned)str[0]%NUM_HASHS]);
}


void luaI_strcallIM (TaggedString *l)
{
  TObject o;
  ttype(&o) = LUA_T_USERDATA;
  for (; l; l=l->next) {
    tsvalue(&o) = l;
    luaI_gcIM(&o);
  }
}


void luaI_strfree (TaggedString *l)
{
  while (l) {
    TaggedString *next = l->next;
    luaI_free(l);
    l = next;
  }
}


/*
** Garbage collection function.
*/
TaggedString *luaI_strcollector (long *acum)
{
  Long counter = 0;
  TaggedString *frees = NULL;
  int i;
  for (i=0; i<NUM_HASHS; i++)
  {
    stringtable *tb = &string_root[i];
    int j;
    for (j=0; j<tb->size; j++)
    {
      TaggedString *t = tb->hash[j];
      if (t != NULL && t->marked <= 1)
      {
        if (t->marked)
          t->marked = 0;
        else
        {
          t->next = frees;
          frees = t;
          tb->hash[j] = &EMPTY;
          counter++;
        }
      }
    }
  }
  *acum += counter;
  return frees;
}

