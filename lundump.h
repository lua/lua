/*
** $Id: lundump.h $
** load precompiled Lua chunks
** See Copyright Notice in lua.h
*/

#ifndef lundump_h
#define lundump_h

#include <limits.h>

#include "llimits.h"
#include "lobject.h"
#include "lzio.h"


/* data to catch conversion errors */
#define LUAC_DATA	"\x19\x93\r\n\x1a\n"

#define LUAC_INT	0x5678
#define LUAC_NUM	cast_num(370.5)

/*
** Encode major-minor version in one byte, one nibble for each
*/
#define LUAC_VERSION	(LUA_VERSION_MAJOR_N*16+LUA_VERSION_MINOR_N)

#define LUAC_FORMAT	0	/* this is the official format */


/*
** Type to handle MSB Varint encoding: Try to get the largest unsigned
** integer available. (It was enough to be the largest between size_t and
** lua_Integer, but the C89 preprocessor knows nothing about size_t.)
*/
#if !defined(LUA_USE_C89) && defined(LLONG_MAX)
typedef unsigned long long varint_t;
#else
typedef unsigned long varint_t;
#endif


/* load one chunk; from lundump.c */
LUAI_FUNC LClosure* luaU_undump (lua_State* L, ZIO* Z, const char* name,
                                               int fixed);

/* dump one chunk; from ldump.c */
LUAI_FUNC int luaU_dump (lua_State* L, const Proto* f, lua_Writer w,
                         void* data, int strip);

#endif
