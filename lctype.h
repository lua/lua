/*
** $Id: lctype.h,v 1.3 2009/03/10 17:42:33 roberto Exp roberto $
** 'ctype' functions for Lua
** See Copyright Notice in lua.h
*/

#ifndef lctype_h
#define lctype_h


#include <limits.h>

#include "lua.h"


#define ALPHABIT	0
#define DIGITBIT	1
#define PRINTBIT	2
#define SPACEBIT	3
#define XDIGITBIT	4


#define MASK(B)		(1 << (B))


/* 'lalpha' (Lua alphabetic) includes '_' */
#define lislalpha(x)	(luai_ctype_[x] & MASK(ALPHABIT))
/* ditto */
#define lislalnum(x)	(luai_ctype_[x] & (MASK(ALPHABIT) | MASK(DIGITBIT)))
#define lisdigit(x)	(luai_ctype_[x] & MASK(DIGITBIT))
#define lisspace(x)	(luai_ctype_[x] & MASK(SPACEBIT))
#define lisprint(x)	(luai_ctype_[x] & MASK(PRINTBIT))
#define lisxdigit(x)	(luai_ctype_[x] & MASK(XDIGITBIT))

LUAI_DATA const char luai_ctype_[UCHAR_MAX + 1];

#endif

