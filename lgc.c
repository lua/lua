/*
** $Id: lgc.c,v 1.34 1999/11/26 18:59:20 roberto Exp roberto $
** Garbage Collector
** See Copyright Notice in lua.h
*/

#define LUA_REENTRANT

#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lref.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lua.h"



static int markobject (lua_State *L, TObject *o);


/* mark a string; marks bigger than 1 cannot be changed */
#define strmark(L, s)    {if ((s)->marked == 0) (s)->marked = 1;}



static void protomark (lua_State *L, TProtoFunc *f) {
  if (!f->marked) {
    int i;
    f->marked = 1;
    strmark(L, f->source);
    for (i=f->nconsts-1; i>=0; i--)
      markobject(L, &f->consts[i]);
  }
}


static void closuremark (lua_State *L, Closure *f) {
  if (!f->marked) {
    int i;
    f->marked = 1;
    for (i=f->nelems; i>=0; i--)
      markobject(L, &f->consts[i]);
  }
}


static void hashmark (lua_State *L, Hash *h) {
  if (!h->marked) {
    int i;
    h->marked = 1;
    for (i=h->size-1; i>=0; i--) {
      Node *n = node(h,i);
      if (ttype(key(n)) != LUA_T_NIL) {
        markobject(L, &n->key);
        markobject(L, &n->val);
      }
    }
  }
}


static void travglobal (lua_State *L) {
  GlobalVar *gv;
  for (gv=L->rootglobal; gv; gv=gv->next) {
    LUA_ASSERT(L, gv->name->u.s.gv == gv, "inconsistent global name");
    if (gv->value.ttype != LUA_T_NIL) {
      strmark(L, gv->name);  /* cannot collect non nil global variables */
      markobject(L, &gv->value);
    }
  }
}


static void travstack (lua_State *L) {
  int i;
  for (i = (L->top-1)-L->stack; i>=0; i--)
    markobject(L, L->stack+i);
}


static void travlock (lua_State *L) {
  int i;
  for (i=0; i<L->refSize; i++) {
    if (L->refArray[i].st == LOCK)
      markobject(L, &L->refArray[i].o);
  }
}


static int markobject (lua_State *L, TObject *o) {
  switch (ttype(o)) {
    case LUA_T_USERDATA:  case LUA_T_STRING:
      strmark(L, tsvalue(o));
      break;
    case LUA_T_ARRAY:
      hashmark(L, avalue(o));
      break;
    case LUA_T_CLOSURE:  case LUA_T_CLMARK:
      closuremark(L, o->value.cl);
      break;
    case LUA_T_PROTO: case LUA_T_PMARK:
      protomark(L, o->value.tf);
      break;
    default: break;  /* numbers, cprotos, etc */
  }
  return 0;
}


static void collectproto (lua_State *L) {
  TProtoFunc **p = &L->rootproto;
  TProtoFunc *next;
  while ((next = *p) != NULL) {
    if (next->marked) {
      next->marked = 0;
      p = &next->next;
    }
    else {
      *p = next->next;
      luaF_freeproto(L, next);
    }
  }
}


static void collectclosure (lua_State *L) {
  Closure **p = &L->rootcl;
  Closure *next;
  while ((next = *p) != NULL) {
    if (next->marked) {
      next->marked = 0;
      p = &next->next;
    }
    else {
      *p = next->next;
      luaF_freeclosure(L, next);
    }
  }
}


static void collecttable (lua_State *L) {
  Hash **p = &L->roottable;
  Hash *next;
  while ((next = *p) != NULL) {
    if (next->marked) {
      next->marked = 0;
      p = &next->next;
    }
    else {
      *p = next->next;
      luaH_free(L, next);
    }
  }
}


/*
** remove from the global list globals whose names will be collected
** (the global itself is freed when its name is freed)
*/
static void clear_global_list (lua_State *L, int limit) {
  GlobalVar **p = &L->rootglobal;
  GlobalVar *next;
  while ((next = *p) != NULL) {
    if (next->name->marked >= limit) p = &next->next;
    else *p = next->next;
  }
}


/*
** collect all elements with `marked' < `limit'.
** with limit=1, that means all unmarked elements;
** with limit=MAX_INT, that means all elements.
*/
static void collectstring (lua_State *L, int limit) {
  TObject o;  /* to call userdata 'gc' tag method */
  int i;
  ttype(&o) = LUA_T_USERDATA;
  clear_global_list(L, limit);
  for (i=0; i<NUM_HASHS; i++) {  /* for each hash table */
    stringtable *tb = &L->string_root[i];
    int j;
    for (j=0; j<tb->size; j++) {  /* for each list */
      TaggedString **p = &tb->hash[j];
      TaggedString *next;
      while ((next = *p) != NULL) {
       if (next->marked >= limit) {
         if (next->marked < FIXMARK)  /* does not change FIXMARKs */
           next->marked = 0;
         p = &next->nexthash;
       } 
       else {  /* collect */
          if (next->constindex == -1) {  /* is userdata? */
            tsvalue(&o) = next;
            luaD_gcIM(L, &o);
          }
          *p = next->nexthash;
          luaS_free(L, next);
          tb->nuse--;
        }
      }
    }
    if ((tb->nuse+1)*6 < tb->size)
      luaS_resize(L, tb, tb->size/2);  /* table is too big */
  }
}


#ifdef LUA_COMPAT_GC
static void tableTM (lua_State *L) {
  Hash *p;
  TObject o;
  ttype(&o) = LUA_T_ARRAY;
  for (p = L->roottable; p; p = p->next) {
    if (!p->marked) {
      avalue(&o) = p;
      luaD_gcIM(L, &o);
    }
  }
}
#else
#define tableTM(L)	/* do nothing */
#endif



static void markall (lua_State *L) {
  travstack(L); /* mark stack objects */
  travglobal(L);  /* mark global variable values and names */
  travlock(L); /* mark locked objects */
  luaT_travtagmethods(L, markobject);  /* mark tag methods */
}


void luaC_collect (lua_State *L, int all) {
  L->GCthreshold *= 4;  /* to avoid GC during GC */
  tableTM(L);  /* call TM for tables (if LUA_COMPAT_GC) */
  collecttable(L);
  collectstring(L, all?MAX_INT:1);
  collectproto(L);
  collectclosure(L);
}


long lua_collectgarbage (lua_State *L, long limit) {
  unsigned long recovered = L->nblocks;  /* to subtract nblocks after gc */
  markall(L);
  luaR_invalidaterefs(L);
  luaC_collect(L, 0);
  luaD_gcIM(L, &luaO_nilobject);  /* GC tag method for nil (signal end of GC) */
  recovered = recovered - L->nblocks;
  L->GCthreshold = (limit == 0) ? 2*L->nblocks : L->nblocks+limit;
  if (L->Mbuffsize > L->Mbuffnext*4) {  /* is buffer too big? */
    L->Mbuffsize /= 2;  /* still larger than Mbuffnext*2 */
    luaM_reallocvector(L, L->Mbuffer, L->Mbuffsize, char);
  }
  return recovered;
}


void luaC_checkGC (lua_State *L) {
  if (L->nblocks >= L->GCthreshold)
    lua_collectgarbage(L, 0);
}

