/*
** $Id: luac.h,v 1.18 2000/10/31 16:57:23 lhf Exp $
** definitions for luac
** See Copyright Notice in lua.h
*/

#include "ldebug.h"
#include "lfunc.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lstring.h"
#include "ltable.h"
#include "lundump.h"

extern lua_State *lua_state;
#define	L	lua_state		/* lazy! */

/* from dump.c */
void luaU_dumpchunk(const Proto* Main, FILE* D);

/* from opt.c */
void luaU_optchunk(Proto* Main);

/* from print.c */
void luaU_printchunk(const Proto* Main);

/* from test.c */
void luaU_testchunk(const Proto* Main);

#define Sizeof(x)	((int)sizeof(x))
