/*
** $Id: ltm.h,v 1.18 2000/10/05 13:00:17 roberto Exp $
** Tag methods
** See Copyright Notice in lua.h
*/

#ifndef ltm_h
#define ltm_h


#include "lobject.h"
#include "lstate.h"

/*
* WARNING: if you change the order of this enumeration,
* grep "ORDER TM"
*/
typedef enum {
  TM_GETTABLE = 0,
  TM_SETTABLE,
  TM_INDEX,
  TM_GETGLOBAL,
  TM_SETGLOBAL,
  TM_ADD,
  TM_SUB,
  TM_MUL,
  TM_DIV,
  TM_POW,
  TM_UNM,
  TM_LT,
  TM_CONCAT,
  TM_GC,
  TM_FUNCTION,
  TM_N		/* number of elements in the enum */
} TMS;


struct TM {
  Closure *method[TM_N];
  TString *collected;  /* list of garbage-collected udata with this tag */
};


#define luaT_gettm(L,tag,event) (L->TMtable[tag].method[event])
#define luaT_gettmbyObj(L,o,e)  (luaT_gettm((L),luaT_tag(o),(e)))


#define validtag(t) (NUM_TAGS <= (t) && (t) <= L->last_tag)

extern const char *const luaT_eventname[];


void luaT_init (lua_State *L);
void luaT_realtag (lua_State *L, int tag);
int luaT_tag (const TObject *o);
int luaT_validevent (int t, int e);  /* used by compatibility module */


#endif
