/*
** $Id: lundump.h,v 1.13 1999/03/29 16:16:18 lhf Exp lhf $
** load pre-compiled Lua chunks
** See Copyright Notice in lua.h
*/

#ifndef lundump_h
#define lundump_h

#include "lobject.h"
#include "lzio.h"

TProtoFunc* luaU_undump1 (ZIO* Z);	/* load one chunk */
void luaU_badconstant (char* s, int i, TObject* o, TProtoFunc* tf);
					/* handle cases that cannot happen */

/* definitions for headers of binary files */
#define	VERSION		0x32		/* last format change was in 3.2 */
#define	VERSION0	0x32		/* last major  change was in 3.2 */
#define ID_CHUNK	27		/* binary files start with ESC... */
#define	SIGNATURE	"Lua"		/* ...followed by this signature */

/* formats for error messages */
#define SOURCE		"<%s:%d>"
#define IN		" in %p " SOURCE
#define INLOC		tf,tf->source->str,tf->lineDefined

/* format for numbers in listings and error messages */
#ifndef NUMBER_FMT
#define NUMBER_FMT	"%.16g"		/* LUA_NUMBER */
#endif

/* LUA_NUMBER
* by default, numbers are stored in precompiled chunks as decimal strings.
* this is completely portable and fast enough for most applications.
* if you want to use this default, do nothing.
* if you want additional speed at the expense of portability, move the line
* below out of this comment.
#define LUAC_NATIVE
*/

#ifdef LUAC_NATIVE
/* a multiple of PI for testing number representation */
/* multiplying by 1E8 gives non-trivial integer values */
#define	TEST_NUMBER	3.14159265358979323846E8
#endif

#endif
