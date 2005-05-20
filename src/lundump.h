/*
** $Id: lundump.h,v 1.35 2005/05/12 00:26:50 lhf Exp $
** load pre-compiled Lua chunks
** See Copyright Notice in lua.h
*/

#ifndef lundump_h
#define lundump_h

#include "lobject.h"
#include "lzio.h"

/* load one chunk; from lundump.c */
LUAI_FUNC Proto* luaU_undump (lua_State* L, ZIO* Z, Mbuffer* buff, const char *name);

/* find byte order; from lundump.c */
LUAI_FUNC int luaU_endianness (void);

/* dump one chunk; from ldump.c */
LUAI_FUNC int luaU_dump (lua_State* L, const Proto* f, lua_Chunkwriter w, void* data, int strip);

/* print one chunk; from print.c */
LUAI_FUNC void luaU_print (const Proto* f, int full);

/* for header of binary files -- this is Lua 5.1 */
#define	VERSION		0x51

/* for testing native format of lua_Numbers */
#define	TEST_NUMBER	((lua_Number)31415926.0)

#endif
