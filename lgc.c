/*
** $Id: lgc.c,v 1.29 1999/10/14 19:13:31 roberto Exp roberto $
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


static void travglobal (void) {
  GlobalVar *gv;
  for (gv=L->rootglobal; gv; gv=gv->next) {
    LUA_ASSERT(gv->name->u.s.gv == gv, "inconsistent global name");
    if (gv->value.ttype != LUA_T_NIL) {
      strmark(gv->name);  /* cannot collect non nil global variables */
      markobject(&gv->value);
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


/*
** remove from the global list globals whose names will be collected
** (the global itself is freed when its name is freed)
*/
static void clear_global_list (int limit) {
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
  travglobal();  /* mark global variable values and names */
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
}


long lua_collectgarbage (long limit) {
  unsigned long recovered = L->nblocks;  /* to subtract nblocks after gc */
  markall();
  luaR_invalidaterefs();
  luaC_collect(0);
  luaD_gcIM(&luaO_nilobject);  /* GC tag method for nil (signal end of GC) */
  recovered = recovered - L->nblocks;
  L->GCthreshold = (limit == 0) ? 2*L->nblocks : L->nblocks+limit;
  return recovered;
}


void luaC_checkGC (void) {
  if (L->nblocks >= L->GCthreshold)
    lua_collectgarbage(0);
}

