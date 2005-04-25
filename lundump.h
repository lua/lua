/*
** $Id: lundump.h,v 1.34 2003/08/25 19:51:54 roberto Exp roberto $
** load pre-compiled Lua chunks
** See Copyright Notice in lua.h
*/

#ifndef lundump_h
#define lundump_h

#include "lobject.h"
#include "lzio.h"

/* load one chunk; from lundump.c */
LUAI_FUNC Proto* luaU_undump (lua_State* L, ZIO* Z, Mbuffer* buff,
                                                    const char *name);

/* find byte order; from lundump.c */
LUAI_FUNC int luaU_endianness (void);

/* dump one chunk; from ldump.c */
LUAI_FUNC int luaU_dump (lua_State* L, const Proto* Main, lua_Chunkwriter w,
                                       void* data, int strip);

/* print one chunk; from print.c */
LUAI_FUNC void luaU_print (const Proto* Main);

/* definitions for headers of binary files */
#define	VERSION		0x50		/* last format change was in 5.0 */
#define	VERSION0	0x50		/* last major  change was in 5.0 */

/* a multiple of PI for testing native format */
/* multiplying by 1E7 gives non-trivial integer values */
#define	TEST_NUMBER	((lua_Number)3.14159265358979323846E7)

#endif
