/*
** $Id: lundump.h,v 1.6 1999/03/30 20:29:34 roberto Exp roberto $
** load pre-compiled Lua chunks
** See Copyright Notice in lua.h
*/

#ifndef lundump_h
#define lundump_h

#include "lobject.h"
#include "lzio.h"

/* -- start of user configuration ------------------------------------------ */

/* LUA_NUMBER
* choose below the number representation in precompiled chunks by choosing an
*  an adequate definition for ID_NUMBER.
* if you change LUA_NUM_TYPE, you must set ID_NUMBER accordingly.
* the default is ID_REAL8 because the default for LUA_NUM_TYPE is double.
* if you want to use this default, do nothing.
* if your machine does not use the IEEE 754 floating-point standard, use
*  ID_NATIVE, but precompiled chunks may not be portable to all architectures.
*
* for types other than the ones listed below, you'll have to write your own
*  dump and undump routines, specially if sizeof(long)!=4.
*/

/* choose one definition for ID_NUMBER and move it to after #endif */
#if 0
#define	ID_NUMBER	ID_INT4		/* 4-byte integers */
#define	ID_NUMBER	ID_REAL4	/* 4-byte reals */
#define	ID_NUMBER	ID_REAL8	/* 8-byte reals */
#define	ID_NUMBER	ID_NATIVE	/* whatever your machine uses */
#endif

/* format for numbers in listings and error messages */
#ifndef NUMBER_FMT
#define NUMBER_FMT	"%.16g"		/* LUA_NUMBER */
#endif

/* -- end of user configuration -- DO NOT CHANGE ANYTHING BELOW THIS LINE -- */


TProtoFunc* luaU_undump1 (ZIO* Z);	/* load one chunk */
void luaU_testnumber (void);		/* test number representation */
void luaU_badconstant (char* s, int i, TObject* o, TProtoFunc* tf);
					/* handle cases that cannot happen */

/* definitions for headers of binary files */
#define	VERSION		0x32		/* last format change was in 3.2 */
#define	VERSION0	0x32		/* last major  change was in 3.2 */
#define ID_CHUNK	27		/* binary files start with ESC... */
#define	SIGNATURE	"Lua"		/* ...followed by this signature */

/* number representation */
#define ID_INT4		'l'		/* 4-byte integers */
#define ID_REAL4	'f'		/* 4-byte reals */
#define ID_REAL8	'd'		/* 8-byte reals */
#define ID_NATIVE	'?'		/* whatever your machine uses */

/* the default is ID_REAL8 because the default for LUA_NUM_TYPE is double */
#ifndef ID_NUMBER
#define	ID_NUMBER	ID_REAL8	/* 8-byte reals */
#endif

/* a multiple of PI for testing number representation */
/* multiplying by 1E8 gives non-trivial integer values */
#define	TEST_NUMBER	3.14159265358979323846E8

#if   ID_NUMBER==ID_REAL4
	#define	DumpNumber	DumpFloat
	#define	LoadNumber	LoadFloat
	#define SIZEOF_NUMBER	4
	#define TYPEOF_NUMBER	float
	#define NAMEOF_NUMBER	"float"
#elif ID_NUMBER==ID_REAL8
	#define	DumpNumber	DumpDouble
	#define	LoadNumber	LoadDouble
	#define SIZEOF_NUMBER	8
	#define TYPEOF_NUMBER	double
	#define NAMEOF_NUMBER	"double"
#elif ID_NUMBER==ID_INT4
	#define	DumpNumber	DumpLong
	#define	LoadNumber	LoadLong
	#define SIZEOF_NUMBER	4
	#define TYPEOF_NUMBER	long
	#define NAMEOF_NUMBER	"long"
#elif ID_NUMBER==ID_NATIVE
	#define	DumpNumber	DumpNative
	#define	LoadNumber	LoadNative
	#define SIZEOF_NUMBER	sizeof(real)
	#define TYPEOF_NUMBER	real
	#define NAMEOF_NUMBER	"native"
#else
	#error	bad ID_NUMBER
#endif

/* formats for error messages */
#define SOURCE		"<%s:%d>"
#define IN		" in %p " SOURCE
#define INLOC		tf,tf->source->str,tf->lineDefined

#endif
