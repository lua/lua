/*
** $Id: fallback.h,v 1.14 1997/02/26 17:38:41 roberto Unstable roberto $
*/
 
#ifndef fallback_h
#define fallback_h

#include "lua.h"
#include "opcode.h"

#define IM_GETTABLE  0
#define IM_ARITH  1
#define IM_ORDER  2
#define IM_CONCAT  3
#define IM_SETTABLE  4
#define IM_GC 5
#define IM_FUNCTION 6
#define IM_INDEX  7
#define IM_N 8

#define GIM_ERROR 0
#define GIM_GETGLOBAL 1
#define GIM_SETGLOBAL 2
#define GIM_N 3

void luaI_setfallback (void);
int luaI_ref (Object *object, int lock);
Object *luaI_getref (int ref);
void luaI_travlock (int (*fn)(Object *));
void luaI_invalidaterefs (void);
char *luaI_travfallbacks (int (*fn)(Object *));

void luaI_settag (int tag, Object *o);
lua_Type luaI_typetag (int tag);
Object *luaI_getim (int tag, int event);
Object *luaI_getgim (int event);
Object *luaI_getimbyObj (Object *o, int event);
int luaI_tag (Object *o);
void luaI_setintmethod (void);
void luaI_setglobalmethod (void);
void luaI_initfallbacks (void);

#endif

