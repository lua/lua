/*
** $Id: lundump.h,v 1.4 1998/01/13 20:05:24 lhf Exp $
** load pre-compiled Lua chunks
** See Copyright Notice in lua.h
*/

#ifndef lundump_h
#define lundump_h

#include "lobject.h"
#include "lzio.h"

#define ID_CHUNK	27		/* ESC */
#define ID_FUNCTION	'#'
#define ID_END		'$'
#define ID_NUM		'N'
#define ID_STR		'S'
#define ID_FUN		'F'
#define	SIGNATURE	"Lua"
#define	VERSION		0x31		/* last format change was in 3.1 */
#define	TEST_FLOAT	0.123456789e-23	/* a float for testing representation */

#define IsMain(f)	(f->lineDefined==0)

TProtoFunc* luaU_undump1(ZIO* Z);	/* load one chunk */

#endif
