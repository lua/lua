/*
** tree.c
** TecCGraf - PUC-Rio
*/
 
char *rcs_tree="$Id: tree.c,v 1.15 1996/01/26 18:03:19 roberto Exp $";


#include <string.h>

#include "mem.h"
#include "lua.h"
#include "tree.h"
#include "hash.h"
#include "table.h"


#define lua_streq(a,b)	(a[0] == b[0] && strcmp(a,b) == 0)

#define NUM_HASHS  64

typedef struct {
  int size;
  int nuse;  /* number of elements (including EMPTYs) */
  TaggedString **hash;
} stringtable;

static stringtable string_root[NUM_HASHS];

static TaggedString EMPTY = {NOT_USED, NOT_USED, 0, 0, {0}};

static unsigned long hash (char *str)
{
  unsigned long h = 0;
  while (*str)
    h = ((h<<5)-h)^(unsigned char)*(str++);
  return h;
}

static void grow (stringtable *tb)
{
  int newsize = luaI_redimension(tb->size);
  TaggedString **newhash = newvector(newsize, TaggedString *);
  int i;
  for (i=0; i<newsize; i++)
    newhash[i] = NULL;
  if (tb->size > 0)
  { /* rehash */
    tb->nuse = 0;
    for (i=0; i<tb->size; i++)
      if (tb->hash[i] != NULL && tb->hash[i] != &EMPTY)
      {
        int h = tb->hash[i]->hash%newsize;
        while (newhash[h])
          h = (h+1)%newsize;
        newhash[h] = tb->hash[i];
        tb->nuse++;
      }
    luaI_free(tb->hash); 
  }
  tb->size = newsize;
  tb->hash = newhash;
}

static TaggedString *insert (char *str, stringtable *tb)
{
  TaggedString *ts;
  unsigned long h = hash(str);
  int i;
  int j = -1;
  if ((Long)tb->nuse*3 >= (Long)tb->size*2)
    grow(tb);
  i = h%tb->size;
  while (tb->hash[i])
  {
    if (tb->hash[i] == &EMPTY)
      j = i;
    else if (lua_streq(str, tb->hash[i]->str))
      return tb->hash[i];
    i = (i+1)%tb->size;
  }
  /* not found */
  if (j != -1)  /* is there an EMPTY space? */
    i = j;
  else
    tb->nuse++;
  ts = tb->hash[i] = (TaggedString *)luaI_malloc(sizeof(TaggedString)+strlen(str));
  strcpy(ts->str, str);
  ts->marked = 0;
  ts->hash = h;
  ts->varindex = ts->constindex = NOT_USED;
  return ts;
}

TaggedString *lua_createstring (char *str) 
{
  return insert(str, &string_root[(unsigned)str[0]%NUM_HASHS]);
}

TaggedString *luaI_createfixedstring (char *str) 
{
  TaggedString *ts = lua_createstring(str);
  ts->marked = 2;  /* to avoid GC */
  return ts;
}

/*
** Garbage collection function.
** This function traverse the string list freeing unindexed strings
*/
Long lua_strcollector (void)
{
  Long counter = 0;
  int i;
  for (i=0; i<NUM_HASHS; i++)
  {
    stringtable *tb = &string_root[i];
    int j;
    for (j=0; j<tb->size; j++)
    {
      TaggedString *t = tb->hash[j];
      if (t != NULL && t != &EMPTY && t->marked != 2)
      {
        if (t->marked)
          t->marked = 0;
        else
        {
          luaI_free(t);
          tb->hash[j] = &EMPTY;
          counter++;
        }
      }
    }
  }
  return counter;
}

