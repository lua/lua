/*
** $Id: lundump.h,v 1.38 2005/09/02 01:54:47 lhf Exp $
** load pre-compiled Lua chunks
** See Copyright Notice in lua.h
*/

#ifndef lundump_h
#define lundump_h

#include "lobject.h"
#include "lzio.h"

/* make it work with Lua 5.0 */
#ifndef LUA_VERSION_NUM
#define LUAI_FUNC
#define lua_Writer	lua_Chunkwriter
#endif

/* load one chunk; from lundump.c */
LUAI_FUNC Proto* luaU_undump (lua_State* L, ZIO* Z, Mbuffer* buff, const char* name);

/* make header; from lundump.c */
LUAI_FUNC void luaU_header (char* h);

/* dump one chunk; from ldump.c */
LUAI_FUNC int luaU_dump (lua_State* L, const Proto* f, lua_Writer w, void* data, int strip);

/* print one chunk; from print.c */
LUAI_FUNC void luaU_print (const Proto* f, int full);

/* for header of binary files -- this is Lua 5.1 */
#define LUAC_VERSION		0x51

/* for header of binary files -- this is the official format */
#define LUAC_FORMAT		0

/* size of header of binary files */
#define LUAC_HEADERSIZE		12

/* make it work with Lua 5.0 */
#ifndef LUA_VERSION_NUM
#define LUA_SIGNATURE		"\033Lua"
#define TValue			TObject
#define rawtsvalue		tsvalue
#define linedefined		lineDefined
#define lastlinedefined		lineDefined
#define setptvalue2s(L,t,f)
#undef	setsvalue2n
#define setsvalue2n(L,x,y)	setsvalue(x,y)
#define LUA_QL(x)		"'" x "'"
#define LUA_QS			LUA_QL("%s")
#undef	LUAC_VERSION
#define LUAC_VERSION		0x50
#ifdef lapi_c
#define luaU_dump(L,f,w,d) 	(luaU_dump)(L,f,w,d,0)
#endif
#ifdef ldo_c
#define luaU_undump(L,z,b)	(luaU_undump)(L,z,b,z->name)
#endif
#endif

#endif
