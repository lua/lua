/*
** $Id: fallback.h,v 1.15 1997/03/19 19:41:10 roberto Exp roberto $
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

#define IM_N 16

extern char *luaI_eventname[];


/*
* WARNING: if you change the order of this enumeration,
* grep "ORDER GIM"
*/
typedef enum {
  GIM_ERROR = 0,
  GIM_GETGLOBAL,
  GIM_SETGLOBAL
} IMGS;

#define GIM_N 3

void luaI_setfallback (void);
int luaI_ref (Object *object, int lock);
Object *luaI_getref (int ref);
void luaI_travlock (int (*fn)(Object *));
void luaI_invalidaterefs (void);
char *luaI_travfallbacks (int (*fn)(Object *));

void luaI_type (void);
void luaI_settag (int tag, Object *o);
lua_Type luaI_typetag (int tag);
Object *luaI_getim (int tag, IMS event);
Object *luaI_getgim (IMGS event);
Object *luaI_getimbyObj (Object *o, IMS event);
int luaI_tag (Object *o);
void luaI_setintmethod (void);
void luaI_setglobalmethod (void);
void luaI_initfallbacks (void);

#endif

