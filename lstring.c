/*
** $Id: lstring.c,v 1.33 2000/03/10 14:38:10 roberto Exp roberto $
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



#define gcsizestring(L, l)	numblocks(L, 0, sizeof(TString)+l)
#define gcsizeudata		gcsizestring(L, 0)



void luaS_init (lua_State *L) {
  int i;
  L->string_root = luaM_newvector(L, NUM_HASHS, stringtable);
  for (i=0; i<NUM_HASHS; i++) {
    L->string_root[i].size = 1;
    L->string_root[i].nuse = 0;
    L->string_root[i].hash = luaM_newvector(L, 1, TString *);;
    L->string_root[i].hash[0] = NULL;
  }
}


void luaS_freeall (lua_State *L) {
  int i;
  for (i=0; i<NUM_HASHS; i++) {
    LUA_ASSERT(L, L->string_root[i].nuse==0, "non-empty string table");
    luaM_free(L, L->string_root[i].hash);
  }
  luaM_free(L, L->string_root);
}


static unsigned long hash_s (const char *s, long l) {
  unsigned long h = l;  /* seed */
  long step = (l>>6)+1;  /* if string is too long, don't hash all its chars */
  for (; l>0; l-=step)
    h = h ^ ((h<<5)+(h>>2)+(unsigned char)*(s++));
  return h;
}


void luaS_resize (lua_State *L, stringtable *tb, int newsize) {
  TString **newhash = luaM_newvector(L, newsize, TString *);
  int i;
  for (i=0; i<newsize; i++) newhash[i] = NULL;
  /* rehash */
  for (i=0; i<tb->size; i++) {
    TString *p = tb->hash[i];
    while (p) {  /* for each node in the list */
      TString *next = p->nexthash;  /* save next */
      int h = p->hash&(newsize-1);  /* new position */
      LUA_ASSERT(L, p->hash%newsize == (p->hash&(newsize-1)),
                    "a&(x-1) == a%x, for x power of 2");
      p->nexthash = newhash[h];  /* chain it in new position */
      newhash[h] = p;
      p = next;
    }
  }
  luaM_free(L, tb->hash);
  tb->size = newsize;
  tb->hash = newhash;
}


static TString *newone (lua_State *L, long l, unsigned long h) {
  TString *ts = (TString *)luaM_malloc(L, 
                                       sizeof(TString)+l*sizeof(char));
  ts->marked = 0;
  ts->nexthash = NULL;
  ts->hash = h;
  return ts;
}


static TString *newone_s (lua_State *L, const char *str,
                               long l, unsigned long h) {
  TString *ts = newone(L, l, h);
  memcpy(ts->str, str, l);
  ts->str[l] = 0;  /* ending 0 */
  ts->u.s.gv = NULL;  /* no global value */
  ts->u.s.len = l;
  ts->constindex = 0;
  L->nblocks += gcsizestring(L, l);
  return ts;
}


static TString *newone_u (lua_State *L, void *buff,
                               int tag, unsigned long h) {
  TString *ts = newone(L, 0, h);
  ts->u.d.value = buff;
  ts->u.d.tag = (tag == LUA_ANYTAG) ? 0 : tag;
  ts->constindex = -1;  /* tag -> this is a userdata */
  L->nblocks += gcsizeudata;
  return ts;
}


static void newentry (lua_State *L, stringtable *tb, TString *ts, int h) {
  ts->nexthash = tb->hash[h];  /* chain new entry */
  tb->hash[h] = ts;
  tb->nuse++;
  if (tb->nuse > tb->size)  /* too crowded? */
    luaS_resize(L, tb, tb->size*2);
}


TString *luaS_newlstr (lua_State *L, const char *str, long l) {
  unsigned long h = hash_s(str, l);
  stringtable *tb = &L->string_root[(l==0) ? 0 :
                         ((unsigned int)(str[0]+str[l-1]))&(NUM_HASHSTR-1)];
  int h1 = h&(tb->size-1);
  TString *ts;
  for (ts = tb->hash[h1]; ts; ts = ts->nexthash) {
    if (ts->u.s.len == l && (memcmp(str, ts->str, l) == 0))
      return ts;
  }
  /* not found */
  ts = newone_s(L, str, l, h);  /* create new entry */
  newentry(L, tb, ts, h1);  /* insert it on table */
  return ts;
}


/*
** uses '%' for one hashing with userdata because addresses are too regular,
** so two '&' operations would be highly correlated
*/
TString *luaS_createudata (lua_State *L, void *udata, int tag) {
  unsigned long h = IntPoint(L, udata);
  stringtable *tb = &L->string_root[(h%NUM_HASHUDATA)+NUM_HASHSTR];
  int h1 = h&(tb->size-1);
  TString *ts;
  for (ts = tb->hash[h1]; ts; ts = ts->nexthash) {
    if (udata == ts->u.d.value && (tag == ts->u.d.tag || tag == LUA_ANYTAG))
      return ts;
  }
  /* not found */
  ts = newone_u(L, udata, tag, h);
  newentry(L, tb, ts, h1);
  return ts;
}


TString *luaS_new (lua_State *L, const char *str) {
  return luaS_newlstr(L, str, strlen(str));
}

TString *luaS_newfixed (lua_State *L, const char *str) {
  TString *ts = luaS_new(L, str);
  if (ts->marked == 0) ts->marked = FIXMARK;  /* avoid GC */
  return ts;
}


void luaS_free (lua_State *L, TString *t) {
  if (t->constindex == -1)  /* is userdata? */
    L->nblocks -= gcsizeudata;
  else {  /* is string */
    L->nblocks -= gcsizestring(L, t->u.s.len);
    luaM_free(L, t->u.s.gv);
  }
  luaM_free(L, t);
}


GlobalVar *luaS_assertglobal (lua_State *L, TString *ts) {
  GlobalVar *gv = ts->u.s.gv;
  if (!gv) {  /* no global value yet? */
    gv = luaM_new(L, GlobalVar);
    gv->value.ttype = TAG_NIL;  /* initial value */
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
  TString *ts = luaS_new(L, name);
  return ts->u.s.gv && ts->u.s.gv->value.ttype != TAG_NIL;
}


