/*
** $Id: lundump.h,v 1.17 1999/12/02 18:51:09 lhf Exp $
** load pre-compiled Lua chunks
** See Copyright Notice in lua.h
*/

#ifndef lundump_h
#define lundump_h

#include "lobject.h"
#include "lzio.h"

/* load one chunk */
TProtoFunc* luaU_undump1 (lua_State* L, ZIO* Z);

/* handle cases that cannot happen */
void luaU_badconstant (lua_State* L, const char* s, int i,
			const TObject* o, const TProtoFunc* tf);

/* convert number from text */
double luaU_str2d (lua_State* L, const char* b, const char* where);

/* definitions for headers of binary files */
#define	VERSION		0x33		/* last format change was in 3.3 */
#define	VERSION0	0x33		/* last major  change was in 3.3 */
#define ID_CHUNK	27		/* binary files start with ESC... */
#define	SIGNATURE	"Lua"		/* ...followed by this signature */

/* formats for error messages */
#define SOURCE		"<%.255s:%d>"
#define IN		" in %p " SOURCE
#define INLOC		tf,tf->source->str,tf->lineDefined

/* format for numbers in listings and error messages */
#ifndef NUMBER_FMT
#define NUMBER_FMT	"%.16g"		/* LUA_NUMBER */
#endif

/* a multiple of PI for testing native format */
/* multiplying by 1E8 gives non-trivial integer values */
#define	TEST_NUMBER	3.14159265358979323846E8

#endif
