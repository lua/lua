/*
** $Id: lundump.h,v 1.9 1998/07/12 00:17:37 lhf Exp $
** load pre-compiled Lua chunks
** See Copyright Notice in lua.h
*/

#ifndef lundump_h
#define lundump_h

#include "lobject.h"
#include "lzio.h"

TProtoFunc* luaU_undump1(ZIO* Z);	/* load one chunk */

#define	SIGNATURE	"Lua"
#define	VERSION		0x31		/* last format change was in 3.1 */
#define	VERSION0	0x31		/* last major  change was in 3.1 */
#define ID_CHUNK	27		/* ESC */

#define IsMain(f)	(f->lineDefined==0)
#define luaO_typename(o)	luaO_typenames[-ttype(o)]

/* number representation */
#define ID_INT4		'l'		/* 4-byte integers */
#define ID_REAL4	'f'		/* 4-byte reals */
#define ID_REAL8	'd'		/* 8-byte reals */
#define ID_NATIVE	'?'		/* whatever your machine uses */

/*
* use a multiple of PI for testing number representation.
* multiplying by 1E8 gives notrivial integer values.
*/
#define	TEST_NUMBER	3.14159265358979323846E8

/* LUA_NUMBER
* choose one below for the number representation in precompiled chunks.
* the default is ID_REAL8 because the default for LUA_NUM_TYPE is double.
* if your machine does not use IEEE 754, use ID_NATIVE.
* the next version will support conversion to/from IEEE 754.
*
* if you change LUA_NUM_TYPE, make sure you set ID_NUMBER accordingly,
* specially if sizeof(long)!=4.
* for types other than the ones listed below, you'll have to write your own
* dump and undump routines.
*/

#ifndef ID_NUMBER
#define	ID_NUMBER	ID_REAL8
#endif

#if 0
#define	ID_NUMBER	ID_INT4
#define	ID_NUMBER	ID_REAL4
#define	ID_NUMBER	ID_REAL8
#define	ID_NUMBER	ID_NATIVE
#endif

#endif

#if   ID_NUMBER==ID_REAL4
	#define	DumpNumber	DumpFloat
	#define	LoadNumber	LoadFloat
	#define SIZEOF_NUMBER	4
#elif ID_NUMBER==ID_REAL8
	#define	DumpNumber	DumpDouble
	#define	LoadNumber	LoadDouble
	#define SIZEOF_NUMBER	8
#elif ID_NUMBER==ID_INT4
	#define	DumpNumber	DumpLong
	#define	LoadNumber	LoadLong
	#define SIZEOF_NUMBER	4
#elif ID_NUMBER==ID_NATIVE
	#define	DumpNumber	DumpNative
	#define	LoadNumber	LoadNative
	#define SIZEOF_NUMBER	sizeof(real)
#else
	#error	bad ID_NUMBER
#endif
