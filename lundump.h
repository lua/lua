/*
** $Id: lundump.h,v 1.19 2000/04/24 17:32:29 lhf Exp lhf $
** load pre-compiled Lua chunks
** See Copyright Notice in lua.h
*/

#ifndef lundump_h
#define lundump_h

#include "lobject.h"
#include "lzio.h"

/* load one chunk */
Proto* luaU_undump (lua_State* L, ZIO* Z);
Proto* luaU_undump1 (lua_State* L, ZIO* Z);

/* find byte order */
int luaU_endianess (void);

/* definitions for headers of binary files */
#define	VERSION		0x40		/* last format change was in 4.0 */
#define	VERSION0	0x40		/* last major  change was in 4.0 */
#define ID_CHUNK	27		/* binary files start with ESC... */
#define	SIGNATURE	"Lua"		/* ...followed by this signature */

/* formats for error messages */
#define xSOURCE		"<%d:%.255s>"
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
