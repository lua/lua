/*
** $Id: lundump.h,v 1.5 1998/02/06 20:05:39 lhf Exp lhf $
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

#define IsMain(f)	(f->lineDefined==0)

#define ID_CHUNK	27		/* ESC */
#define ID_NUM		'N'
#define ID_STR		'S'
#define ID_FUN		'F'

#define ID_INT4		'l'
#define ID_REAL4	'f'
#define ID_REAL8	'd'
#define ID_NATIVE	'?'

/*
* use a multiple of PI for testing number representation.
* multiplying by 10E8 gives notrivial integer values.
*/
#define	TEST_NUMBER	3.14159265358979323846E8

/* LUA_NUMBER */
/* if you change the definition of real, make sure you set ID_NUMBER
* accordingly, specially if sizeof(long)!=4.
* for types other than the ones listed below, you'll have to write your own
* dump and undump routines.
*/

#if real==float
	#define	ID_NUMBER	ID_REAL4
#elif real==double
	#define	ID_NUMBER	ID_REAL8
#elif real==long
	#define	ID_NUMBER	ID_INT4
#else
	#define	ID_NUMBER	ID_NATIVE
#endif

#endif
