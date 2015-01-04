/*
** undump.h
** definitions for lua decompiler
** $Id: undump.h,v 1.2 1996/03/11 21:59:41 lhf Exp $
*/

#include "func.h"

#define IsMain(f)	(f->lineDefined==0)

/* definitions for chunk headers */

#define ID_CHUNK	27		/* ESC */
#define ID_FUN		'F'
#define ID_VAR		'V'
#define ID_STR		'S'
#define	SIGNATURE	"Lua"
#define	VERSION		0x23		/* 2.3 */
#define	TEST_WORD	0x1234		/* a word for testing byte ordering */
#define	TEST_FLOAT	0.123456789e-23	/* a float for testing representation */

TFunc* luaI_undump1(FILE* D);		/* load one chunk */
int luaI_undump(FILE* D);		/* load all chunks */
