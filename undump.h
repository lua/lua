/*
** undump.h
** definitions for lua decompiler
** $Id: undump.h,v 1.4 1997/04/14 12:12:40 lhf Exp roberto $
*/

#include "func.h"
#include "zio.h"

#define IsMain(f)	(f->lineDefined==0)

/* definitions for chunk headers */

#define ID_CHUNK	27		/* ESC */
#define ID_FUN		'F'
#define ID_VAR		'V'
#define ID_STR		'S'
#define	SIGNATURE	"Lua"
#define	VERSION		0x25		/* last format change was in 2.5 */
#define	TEST_WORD	0x1234		/* a word for testing byte ordering */
#define	TEST_FLOAT	0.123456789e-23	/* a float for testing representation */


int luaI_undump(ZIO* Z);		/* load all chunks */
