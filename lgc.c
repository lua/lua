/*
** $Id: lgc.c,v 1.28 1999/10/11 16:13:11 roberto Exp roberto $
** Garbage Collector
** See Copyright Notice in lua.h
*/


#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lobject.h"
#include "lref.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lua.h"



static int markobject (TObject *o);


/* mark a string; marks bigger than 1 cannot be changed */
#define strmark(s)    {if ((s)->marked == 0) (s)->marked = 1;}



static void protomark (TProtoFunc *f) {
  if (!f->marked) {
    int i;
    f->marked = 1;
    strmark(f->source);
    for (i=f->nconsts-1; i>=0; i--)
      markobject(&f->consts[i]);
  }
}


static void closuremark (Closure *f) {
  if (!f->marked) {
    int i;
    f->marked = 1;
    for (i=f->nelems; i>=0; i--)
      markobject(&f->consts[i]);
  }
}


static void hashmark (Hash *h) {
  if (!h->marked) {
    int i;
    h->marked = 1;
    for (i=h->size-1; i>=0; i--) {
      Node *n = node(h,i);
      if (ttype(key(n)) != LUA_T_NIL) {
        markobject(&n->key);
        markobject(&n->val);
      }
    }
  }
}


static void globalmark (void) {
  TaggedString *g;
  for (g=L->rootglobal; g; g=g->nextglobal) {
    LUA_ASSERT(g->constindex >= 0, "userdata in global list");
    if (g->u.s.globalval.ttype != LUA_T_NIL) {
      markobject(&g->u.s.globalval);
      strmark(g);  /* cannot collect non nil global variables */
    }
  }
}


static void travstack (void) {
  StkId i;
  for (i = (L->stack.top-1)-L->stack.stack; i>=0; i--)
    markobject(L->stack.stack+i);
}


static void travlock (void) {
  int i;
  for (i=0; i<L->refSize; i++)
    if (L->refArray[i].status == LOCK)
      markobject(&L->refArray[i].o);
}


static int markobject (TObject *o) {
  switch (ttype(o)) {
    case LUA_T_USERDATA:  case LUA_T_STRING:
      strmark(tsvalue(o));
      break;
    case LUA_T_ARRAY:
      hashmark(avalue(o));
      break;
    case LUA_T_CLOSURE:  case LUA_T_CLMARK:
      closuremark(o->value.cl);
      break;
    case LUA_T_PROTO: case LUA_T_PMARK:
      protomark(o->value.tf);
      break;
    default: break;  /* numbers, cprotos, etc */
  }
  return 0;
}


static void collectproto (void) {
  TProtoFunc **p = &L->rootproto;
  TProtoFunc *next;
  while ((next = *p) != NULL) {
    if (next->marked) {
      next->marked = 0;
      p = &next->next;
    }
    else {
      *p = next->next;
      luaF_freeproto(next);
    }
  }
}


static void collectclosure (void) {
  Closure **p = &L->rootcl;
  Closure *next;
  while ((next = *p) != NULL) {
    if (next->marked) {
      next->marked = 0;
      p = &next->next;
    }
    else {
      *p = next->next;
      luaF_freeclosure(next);
    }
  }
}


static void collecttable (void) {
  Hash **p = &L->roottable;
  Hash *next;
  while ((next = *p) != NULL) {
    if (next->marked) {
      next->marked = 0;
      p = &next->next;
    }
    else {
      *p = next->next;
      luaH_free(next);
    }
  }
}


static void clear_global_list (int limit) {
  TaggedString **p = &L->rootglobal;
  TaggedString *next;
  while ((next = *p) != NULL) {
    if (next->marked >= limit) p = &next->nextglobal;
    else *p = next->nextglobal;
  }
}


/*
** collect all elements with `marked' < `limit'.
** with limit=1, that means all unmarked elements;
** with limit=MAX_INT, that means all elements (but EMPTY).
*/
static void collectstring (int limit) {
  TObject o;  /* to call userdata 'gc' tag method */
  int i;
  ttype(&o) = LUA_T_USERDATA;
  clear_global_list(limit);
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
            luaD_gcIM(&o);
          }
          *p = next->nexthash;
          luaS_free(next);
          tb->nuse--;
        }
      }
    }
    if ((tb->nuse+1)*6 < tb->size)
      luaS_grow(tb);  /* table is too big; `grow' it to a smaller size */
  }
}


#ifdef LUA_COMPAT_GC
static void tableTM (void) {
  Hash *p;
  TObject o;
  ttype(&o) = LUA_T_ARRAY;
  for (p = L->roottable; p; p = p->next) {
    if (!p->marked) {
      avalue(&o) = p;
      luaD_gcIM(&o);
    }
  }
}
#else
#define tableTM()	/* do nothing */
#endif



static void markall (void) {
  travstack(); /* mark stack objects */
  globalmark();  /* mark global variable values and names */
  travlock(); /* mark locked objects */
  luaT_travtagmethods(markobject);  /* mark tag methods */
}


void luaC_collect (int all) {
  L->GCthreshold *= 4;  /* to avoid GC during GC */
  tableTM();  /* call TM for tables (if LUA_COMPAT_GC) */
  collecttable();
  collectstring(all?MAX_INT:1);
  collectproto();
  collectclosure();
  if (!all)
    luaD_gcIM(&luaO_nilobject);  /* GC tag method for nil (signal end of GC) */
}


long lua_collectgarbage (long limit) {
  unsigned long recovered = L->nblocks;  /* to subtract nblocks after gc */
  markall();
  luaR_invalidaterefs();
  luaC_collect(0);
  recovered = recovered - L->nblocks;
  L->GCthreshold = (limit == 0) ? 2*L->nblocks : L->nblocks+limit;
  return recovered;
}


void luaC_checkGC (void) {
  if (L->nblocks >= L->GCthreshold)
    lua_collectgarbage(0);
}

