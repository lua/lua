/*
** $Id: lundump.h,v 1.33 2004/06/09 21:03:53 lhf Exp $
** load pre-compiled Lua chunks
** See Copyright Notice in lua.h
*/

#ifndef lundump_h
#define lundump_h

#include "lobject.h"
#include "lzio.h"

/* load one chunk; from lundump.c */
Proto* luaU_undump (lua_State* L, ZIO* Z, Mbuffer* buff, const char *name);

/* find byte order; from lundump.c */
int luaU_endianness (void);

/* dump one chunk; from ldump.c */
int luaU_dump (lua_State* L, const Proto* f, lua_Chunkwriter w, void* data, int strip);

/* print one chunk; from print.c */
void luaU_print (const Proto* f, int full);

/* definitions for headers of binary files */
#define	VERSION		0x51		/* last format change was in 5.1 */
#define	VERSION0	0x51		/* last major  change was in 5.1 */

/* a multiple of PI for testing native format */
/* multiplying by 1E7 gives non-trivial integer values */
#define	TEST_NUMBER	((lua_Number)3.14159265358979323846E7)

#endif
