/*
** $Id: lundump.h,v 1.24 2002/06/03 17:46:34 roberto Exp roberto $
** load pre-compiled Lua chunks
** See Copyright Notice in lua.h
*/

#ifndef lundump_h
#define lundump_h

#include "lobject.h"
#include "lzio.h"

/* load one chunk */
Proto* luaU_undump (lua_State* L, ZIO* Z);

/* find byte order */
int luaU_endianness (void);

/* definitions for headers of binary files */
#define	VERSION		0x41		/* last format change was in 4.1 */
#define	VERSION0	0x41		/* last major  change was in 4.1 */

/* a multiple of PI for testing native format */
/* multiplying by 1E8 gives non-trivial integer values */
#define	TEST_NUMBER	3.14159265358979323846E8

/* binary files start with <esc>Lua */
#define	LUA_SIGNATURE	"\033Lua"

#endif
