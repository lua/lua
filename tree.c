/*
** tree.c
** TecCGraf - PUC-Rio
*/
 
char *rcs_tree="$Id: tree.c,v 1.24 1997/03/31 14:17:09 roberto Exp roberto $";


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

static TaggedString EMPTY = {LUA_T_STRING, 0, NOT_USED, NOT_USED, 0, 2, {0}};


static unsigned long hash (char *buff, long size)
{
  unsigned long h = 0;
  while (size--)
    h = ((h<<5)-h)^(unsigned char)*(buff++);
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

static TaggedString *insert (char *buff, long size, int tag, stringtable *tb)
{
  TaggedString *ts;
  unsigned long h = hash(buff, size);
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
    else if (ts->size == size && ts->tag == tag &&
             memcmp(buff, ts->str, size) == 0)
      return ts;
    i = (i+1)%tb->size;
  }
  /* not found */
  lua_pack();
  if (j != -1)  /* is there an EMPTY space? */
    i = j;
  else
    tb->nuse++;
  ts = tb->hash[i] = (TaggedString *)luaI_malloc(sizeof(TaggedString)+size-1);
  memcpy(ts->str, buff, size);
  ts->tag = tag;
  ts->size = size;
  ts->marked = 0;
  ts->hash = h;
  ts->varindex = ts->constindex = NOT_USED;
  return ts;
}

TaggedString *luaI_createuserdata (char *buff, long size, int tag)
{
  return insert(buff, size, tag, &string_root[(unsigned)buff[0]%NUM_HASHS]);
}

TaggedString *lua_createstring (char *str)
{
  return luaI_createuserdata(str, strlen(str)+1, LUA_T_STRING);
}


void luaI_strcallIM (void)
{
  int i;
  TObject o;
  ttype(&o) = LUA_T_USERDATA;
  for (i=0; i<NUM_HASHS; i++) {
    stringtable *tb = &string_root[i];
    int j;
    for (j=0; j<tb->size; j++) {
      TaggedString *t = tb->hash[j];
      if (t != NULL && t->tag != LUA_T_STRING && t->marked == 0) {
        tsvalue(&o) = t;
        luaI_gcIM(&o);
      }
    }
  }
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
      if (t != NULL && t->marked <= 1)
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

