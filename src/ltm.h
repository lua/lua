/*
** $Id: ltm.h,v 1.5 1999/01/15 13:11:57 roberto Exp $
** Tag methods
** See Copyright Notice in lua.h
*/

#ifndef ltm_h
#define ltm_h


#include "lobject.h"
#include "lstate.h"

/*
* WARNING: if you change the order of this enumeration,
* grep "ORDER IM"
*/
typedef enum {
  IM_GETTABLE = 0,
  IM_SETTABLE,
  IM_INDEX,
  IM_GETGLOBAL,
  IM_SETGLOBAL,
  IM_ADD,
  IM_SUB,
  IM_MUL,
  IM_DIV,
  IM_POW,
  IM_UNM,
  IM_LT,
  IM_LE,
  IM_GT,
  IM_GE,
  IM_CONCAT,
  IM_GC,
  IM_FUNCTION
} IMS;

#define IM_N 18


struct IM {
  TObject int_method[IM_N];
};


#define luaT_getim(tag,event) (&L->IMtable[-(tag)].int_method[event])
#define luaT_getimbyObj(o,e)  (luaT_getim(luaT_effectivetag(o),(e)))

extern char *luaT_eventname[];


void luaT_init (void);
void luaT_realtag (int tag);
int luaT_effectivetag (TObject *o);
void luaT_settagmethod (int t, char *event, TObject *func);
TObject *luaT_gettagmethod (int t, char *event);
char *luaT_travtagmethods (int (*fn)(TObject *));

void luaT_setfallback (void);  /* only if LUA_COMPAT2_5 */

#endif
