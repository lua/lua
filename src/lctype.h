/*
** $Id: lctype.h,v 1.8 2009/11/19 19:06:52 roberto Exp $
** 'ctype' functions for Lua
** See Copyright Notice in lua.h
*/

#ifndef lctype_h
#define lctype_h


#include <limits.h>

#include "lua.h"

#include "llimits.h"


#define ALPHABIT	0
#define DIGITBIT	1
#define PRINTBIT	2
#define SPACEBIT	3
#define XDIGITBIT	4
#define UPPERBIT	5


#define MASK(B)		(1 << (B))


/*
** add 1 to char to allow index -1 (EOZ)
*/
#define testprop(c,p)	(luai_ctype_[(c)+1] & (p))

/*
** 'lalpha' (Lua alphabetic) and 'lalnum' (Lua alphanumeric) both include '_'
*/
#define lislalpha(c)	testprop(c, MASK(ALPHABIT))
#define lislalnum(c)	testprop(c, (MASK(ALPHABIT) | MASK(DIGITBIT)))
#define lisupper(c)	testprop(c, MASK(UPPERBIT))
#define lisdigit(c)	testprop(c, MASK(DIGITBIT))
#define lisspace(c)	testprop(c, MASK(SPACEBIT))
#define lisprint(c)	testprop(c, MASK(PRINTBIT))
#define lisxdigit(c)	testprop(c, MASK(XDIGITBIT))


/* one more entry for 0 and one more for -1 (EOZ) */
LUAI_DDEC const lu_byte luai_ctype_[UCHAR_MAX + 2];

#endif

