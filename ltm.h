/*
** $Id: ltm.h,v 1.34 2002/06/12 14:51:31 roberto Exp roberto $
** Tag methods
** See Copyright Notice in lua.h
*/

#ifndef ltm_h
#define ltm_h


#include "lobject.h"

/*
* WARNING: if you change the order of this enumeration,
* grep "ORDER TM"
*/
typedef enum {
  TM_GETTABLE = 0,
  TM_SETTABLE,
  TM_INDEX,
  TM_NEWINDEX,
  TM_GC,
  TM_EQ,
  TM_WEAKMODE,  /* last tag method with `fast' access */
  TM_ADD,
  TM_SUB,
  TM_MUL,
  TM_DIV,
  TM_POW,
  TM_UNM,
  TM_LT,
  TM_LE,
  TM_CONCAT,
  TM_CALL,
  TM_N		/* number of elements in the enum */
} TMS;



#define fasttm(l,et,e) \
  (((et)->flags & (1u<<(e))) ? NULL : luaT_gettm(et, e, G(l)->tmname[e]))


const TObject *luaT_gettm (Table *events, TMS event, TString *ename);
const TObject *luaT_gettmbyobj (lua_State *L, const TObject *o, TMS event);
void luaT_init (lua_State *L);

extern const char *const luaT_typenames[];

#endif
