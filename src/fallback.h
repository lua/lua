/*
** $Id: fallback.h,v 1.23 1997/04/24 22:59:57 roberto Exp $
*/
 
#ifndef fallback_h
#define fallback_h

#include "lua.h"
#include "opcode.h"

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


extern struct IM {
  TObject int_method[IM_N];
} *luaI_IMtable;

extern char *luaI_eventname[];

#define luaI_getim(tag,event) (&luaI_IMtable[-(tag)].int_method[event])
#define luaI_getimbyObj(o,e)  (luaI_getim(luaI_efectivetag(o),(e)))

void luaI_setfallback (void);
int luaI_ref (TObject *object, int lock);
TObject *luaI_getref (int ref);
void luaI_travlock (int (*fn)(TObject *));
void luaI_invalidaterefs (void);
char *luaI_travfallbacks (int (*fn)(TObject *));

void luaI_settag (int tag, TObject *o);
void luaI_realtag (int tag);
TObject *luaI_geterrorim (void);
int luaI_efectivetag (TObject *o);
void luaI_settagmethod (void);
void luaI_gettagmethod (void);
void luaI_seterrormethod (void);
void luaI_initfallbacks (void);

#endif

